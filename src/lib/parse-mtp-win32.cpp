/***************************************************************************
 *   Copyright (C) 2009-2010 by Robert Keevil                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; under version 2 of the License.         *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include <Propidl.h> //required for the date value in get_metadata
#include "parse-mtp.h"

Parse_MTP::Parse_MTP()
{
    qDebug() << "Parse_MTP::Parse_MTP";
    tz_offset = 0;
}

Parse_MTP::~Parse_MTP()
{
    qDebug() << "Parse_MTP::~Parse_MTP";
}

void Parse_MTP::open(QString file_path, int tz)
{
    tracks = playcounts = entries = 0;

    tz_offset = tz;
    LPWSTR *pnp_device_ids = NULL;

    mtp_devices.clear();

    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceManager,
                                  NULL,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pdm));

    if (FAILED(hr))
    {
        emit open_finished(false, tr("Failed to CoCreate: %1")
                           .arg(hr, 0, 16));
        return;
    }

    DWORD num_devices;
    hr = pdm->GetDevices(NULL, &num_devices);
    if (FAILED(hr))
    {
        emit open_finished(false, tr("Failed to get number of devices: %1")
                           .arg(hr, 0, 16));
        return;
    }

    emit add_log(LOG_INFO, QString("MTP: Found %1 devices").arg(num_devices));

    pnp_device_ids = new (std::nothrow) LPWSTR[num_devices];

    if (pnp_device_ids != NULL && num_devices > 0)
    {
        hr = pdm->GetDevices(pnp_device_ids, &num_devices);
        if (FAILED(hr))
        {
            emit open_finished(false, tr("MTP: Failed to get devices: %1")
                               .arg(hr, 0, 16));
            return;
        }

        for (DWORD i = 0; i < num_devices; i++)
        {
            mtp_device_name(i, pnp_device_ids[i]);
            // Check to see if this is an MTP device
			if (is_device_mtp(i, pnp_device_ids[i]))
				mtp_devices.push_back(pnp_device_ids[i]);
        }

        delete [] pnp_device_ids;
    }

    if (0 == mtp_devices.count())
    {
        emit open_finished(false, tr("MTP: No devices have been found"));
        return;
    }

    // So now we have a list of MTP devices - time to dig a little deeper
    // and recurse through all the objects within.
    mtp_do_storage(false);

    emit add_log(LOG_INFO,
                 QString("Found %1 entries, %2 of %3 tracks had playcount information")
                 .arg(entries)
                 .arg(playcounts)
                 .arg(tracks));

    emit open_finished(true, tr("MTP: Finished parsing tracks"));
}

void Parse_MTP::clear()
{
    mtp_do_storage(true);
    emit clear_finished(true, tr("MTP: Finished clearing tracks"));
}

bool Parse_MTP::is_device_mtp(DWORD index, LPCWSTR device_id)
{
    bool is_mtp = false;

    IPortableDevice *pd;
    IPortableDeviceValues *pdv;
    IPortableDeviceKeyCollection *pdkc;
    IPortableDeviceProperties *pdp = NULL;
    IPortableDeviceContent *pdc = NULL;
    HRESULT hr;
    LPWSTR dev_protocol = NULL;
    QString mtp;


    hr = CoCreateInstance(CLSID_PortableDevice, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDevice, (VOID**)&pd);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to create IPortableDevice: %1")
                .arg(hr, 0, 16));
        return false;
    }

    hr = CoCreateInstance(CLSID_PortableDeviceValues, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDeviceValues, (VOID**)&pdv);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to create IPortableDeviceValues: %1")
                .arg(hr, 0, 16));
        return false;
    }

    hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDeviceKeyCollection, (VOID**)&pdkc);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to create IPortableDeviceKeyCollection: %1")
                .arg(hr, 0, 16));
        return false;
    }

    // after this we're creating objects which need to be cleaned up.
    hr = pd->Open(device_id, pdv);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to open IPortableDevice: %1")
                .arg(hr, 0, 16));
        goto is_mtp_cleanup;
    }

    hr = pd->Content(&pdc);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to retrieve content from IPortableDevice: %1")
                .arg(hr, 0, 16));
        goto is_mtp_cleanup;
    }

    hr = pdc->Properties(&pdp);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to get properties from IPortableDeviceContent: %1")
                .arg(hr, 0, 16));
        goto is_mtp_cleanup;
    }

    hr = pdkc->Add(WPD_DEVICE_PROTOCOL);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to add to IPortableDeviceKeyCollection: %1")
                .arg(hr, 0, 16));
        goto is_mtp_cleanup;
    }

    // WPD_DEVICE_OBJECT_ID is the top level object
    hr = pdp->GetValues(WPD_DEVICE_OBJECT_ID, pdkc, &pdv);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to get values from IPortableDeviceProperties: %1")
                .arg(hr, 0, 16));
        goto is_mtp_cleanup;
    }

    hr = pdv->GetStringValue(WPD_DEVICE_PROTOCOL, &dev_protocol);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("is_device_mtp: Failed to GetStringValue: %1")
                .arg(hr, 0, 16));
        goto is_mtp_cleanup;
    }

    mtp = QString::fromStdWString(dev_protocol);

    emit add_log(LOG_INFO, QString("Device %1: %2").arg(index).arg(mtp));
    if (mtp.startsWith("MTP"))
    {
        is_mtp = true;
        emit add_log(LOG_INFO, QString("Device %1: Is a MTP device").arg(index));
    }

is_mtp_cleanup:
    if (dev_protocol)
        CoTaskMemFree(dev_protocol);
    if (pdc)
    {
        pdc->Release();
        pdc = NULL;
    }
    if (pdp)
    {
        pdp->Release();
        pdp = NULL;
    }
    if (pdv)
    {
        pdv->Release();
        pdv = NULL;
    }

    return is_mtp;
}

void Parse_MTP::mtp_device_name(DWORD index, LPCWSTR device_id)
{
    // model is sometimes the "friendly" name, or just manufacturers model name
    DWORD name_len = 256;
    WCHAR *name = new (std::nothrow) WCHAR[name_len];
    QString make, model, desc;

    HRESULT hr = pdm->GetDeviceManufacturer(device_id, name, &name_len);
    if (SUCCEEDED(hr))
        make = QString::fromWCharArray(name, name_len);

    // Reset name_len back to the starting size
    name_len = 256;
    hr = pdm->GetDeviceFriendlyName(device_id, name, &name_len);
    if (SUCCEEDED(hr))
        model = QString::fromWCharArray(name, name_len);

    name_len = 256;
    hr = pdm->GetDeviceDescription(device_id, name, &name_len);
    if (SUCCEEDED(hr))
        desc = QString::fromWCharArray(name, name_len);
    delete [] name;

    emit add_log(LOG_INFO,
                 QString("Device %1: %2 - %3 - %4")
                 .arg(index)
                 .arg(make, model, desc));
}

void Parse_MTP::mtp_do_storage(bool clear)
{
    IPortableDevice *pd;
    IPortableDeviceValues *pdv;
    IPortableDeviceContent *pdc = NULL;
    HRESULT hr;

    hr = CoCreateInstance(CLSID_PortableDevice, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDevice, (VOID**)&pd);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_do_storage: Failed to create IPortableDevice: %1")
                .arg(hr, 0, 16));
        return;
    }

    hr = CoCreateInstance(CLSID_PortableDeviceValues, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDeviceValues, (VOID**)&pdv);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_do_storage: Failed to create IPortableDeviceValues: %1")
                .arg(hr, 0, 16));
        return;
    }

    for (int i = 0; i < mtp_devices.size(); i++)
    {
        // after this we're creating objects which need to be cleaned up.
        hr = pd->Open(mtp_devices[i], pdv);
        if (FAILED(hr))
        {
            emit add_log(LOG_ERROR, QString("mtp_do_storage: Failed to open IPortableDevice: %1")
                    .arg(hr, 0, 16));
            goto mtp_do_storage_cleanup;
        }

        hr = pd->Content(&pdc);
        if (FAILED(hr))
        {
            emit add_log(LOG_ERROR, QString("mtp_do_storage: Failed to retrieve content from IPortableDevice: %1")
                    .arg(hr, 0, 16));
            goto mtp_do_storage_cleanup;
        }
        mtp_recurse_storage(WPD_DEVICE_OBJECT_ID, pdc, clear);

mtp_do_storage_cleanup:
        if (pdc)
        {
            pdc->Release();
            pdc = NULL;
        }
        if (pdv)
        {
            pdv->Release();
            pdv = NULL;
        }
        pd->Close();
    }
}

void Parse_MTP::mtp_recurse_storage(PCWSTR object_id, IPortableDeviceContent *pdc, bool clear)
{
    IEnumPortableDeviceObjectIDs *enum_ids = NULL;
    HRESULT hr;

    hr = pdc->EnumObjects(0, object_id, NULL, &enum_ids);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_recurse_storage: Failed to get objects from PDC: %1")
                .arg(hr, 0, 16));
        return;
    }

    mtp_get_metadata(object_id, pdc, clear);

    while (S_OK == hr)
    {
        const int batch_size = 10;
        ULONG num_fetched = 0;
        PWSTR object_ids[batch_size] = {0};
        hr = enum_ids->Next(batch_size, object_ids, &num_fetched);
        if (SUCCEEDED(hr))
        {
            for (ULONG i = 0; i < num_fetched; i++)
            {
                mtp_recurse_storage(object_ids[i], pdc, clear);

                CoTaskMemFree(object_ids[i]);
                object_ids[i] = NULL;
            }
        }
    }
}

void Parse_MTP::mtp_get_metadata(PCWSTR object_id, IPortableDeviceContent *pdc, bool clear)
{
    IPortableDeviceValues *pdv;
    IPortableDeviceKeyCollection *pdkc;
    IPortableDeviceProperties *pdp = NULL;
    HRESULT hr;
    scrob_entry new_entry;
    LPWSTR album = NULL;
    LPWSTR artist = NULL;
    LPWSTR name = NULL;
    ULONG usecount;
    ULONG skipcount;
    ULONG tracknum;
    ULONGLONG duration;
    PROPVARIANT date;

    new_entry.length = 0;

    hr = CoCreateInstance(CLSID_PortableDeviceValues, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDeviceValues, (VOID**)&pdv);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_get_metadata: Failed to create IPortableDeviceValues: %1")
                .arg(hr, 0, 16));
        return;
    }

    hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER,
                          IID_IPortableDeviceKeyCollection, (VOID**)&pdkc);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_get_metadata: Failed to create IPortableDeviceKeyCollection: %1")
                .arg(hr, 0, 16));
        return;
    }

    hr = pdc->Properties(&pdp);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_get_metadata: Failed to get properties from IPortableDeviceContent: %1")
                .arg(hr, 0, 16));
        goto mtp_get_metadata_cleanup;
    }

    hr = pdkc->Add(WPD_MEDIA_USE_COUNT);          //VT_UI4
    hr = pdkc->Add(WPD_MEDIA_SKIP_COUNT);         //VT_UI4
    hr = pdkc->Add(WPD_MEDIA_LAST_ACCESSED_TIME); //VT_DATE
    hr = pdkc->Add(WPD_MUSIC_ALBUM);              //VT_LPWSTR
    hr = pdkc->Add(WPD_MUSIC_TRACK);              //VT_UI4
    hr = pdkc->Add(WPD_MEDIA_DURATION);           //VT_UI8, in milliseconds
    hr = pdkc->Add(WPD_OBJECT_NAME);              //VT_LPWSTR
    hr = pdkc->Add(WPD_MEDIA_ARTIST);             //VT_LPWSTR
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_get_metadata: Failed to add to IPortableDeviceKeyCollection: %1")
                .arg(hr, 0, 16));
        goto mtp_get_metadata_cleanup;
    }

    hr = pdp->GetValues(object_id, pdkc, &pdv);
    if (FAILED(hr))
    {
        emit add_log(LOG_ERROR, QString("mtp_get_metadata: Failed to get values from IPortableDeviceProperties: %1")
                .arg(hr, 0, 16));
        goto mtp_get_metadata_cleanup;
    }

    hr = pdv->GetStringValue(WPD_MUSIC_ALBUM, &album);
    if (SUCCEEDED(hr))
        new_entry.album = QString::fromStdWString(album);

    hr = pdv->GetStringValue(WPD_MEDIA_ARTIST, &artist);
    if (SUCCEEDED(hr))
        new_entry.artist = QString::fromStdWString(artist);

    hr = pdv->GetStringValue(WPD_OBJECT_NAME, &name);
    if (SUCCEEDED(hr))
        new_entry.title = QString::fromStdWString(name);

    hr = pdv->GetUnsignedIntegerValue(WPD_MEDIA_USE_COUNT, &usecount);
    hr = pdv->GetUnsignedIntegerValue(WPD_MEDIA_SKIP_COUNT, &skipcount);

    hr = pdv->GetUnsignedIntegerValue(WPD_MUSIC_TRACK, &tracknum);
    if (SUCCEEDED(hr))
        new_entry.tracknum = tracknum;

    hr = pdv->GetUnsignedLargeIntegerValue(WPD_MEDIA_DURATION, &duration);
    if (SUCCEEDED(hr))
        new_entry.length = duration/1000;

    hr = pdv->GetValue(WPD_MEDIA_LAST_ACCESSED_TIME, &date);
    if (SUCCEEDED(hr))
    {
        SYSTEMTIME st;
        VariantTimeToSystemTime(date.date, &st);
        PropVariantClear(&date);
        QDate d = QDate(st.wYear, st.wMonth, st.wDay);
        QTime t = QTime(st.wHour, st.wMinute, st.wSecond);
        new_entry.when = QDateTime(d, t).toTime_t() - tz_offset;

        // Not all MTP devices provide a valid result for
        // WPD_MEDIA_LAST_ACCESSED_TIME, so sanity check it

        // are we within (+/-) 30 days?
        long offset = new_entry.when - QDateTime::currentDateTime().toTime_t();
        if (abs(offset) > 2592000)
            new_entry.when = 0;
    }

    emit add_log(LOG_TRACE, 
        QString("%1 : %2 : %3 : %4 : %5 : %6 : %7")
        .arg(new_entry.artist)
        .arg(new_entry.title)
        .arg(new_entry.album)
        .arg(new_entry.tracknum)
        .arg(new_entry.length)
        .arg(usecount)
        .arg(skipcount)
        );

    tracks++;

    if (usecount > 0 
        && new_entry.artist.length() 
        && new_entry.title.length())
    {
        if (clear)
        {
            IPortableDeviceValues *pdv_clear = NULL;
            hr = pdv->Clear();
            hr = pdv->SetUnsignedIntegerValue(WPD_MEDIA_USE_COUNT, 0);
            hr = pdp->SetValues(object_id, pdv, &pdv_clear);
            if (SUCCEEDED(hr))
            {
                if (pdv_clear != NULL)
                    pdv_clear->Clear();
                    pdv_clear = NULL;
            }
        }
        else
        {
            playcounts++;
            new_entry.played = 'L';

            for (ULONG i = 0; i < usecount; i++)
            {
                entries++;

                // Only use the last playcount for one instance
                // we'll recalc the rest afterwards
                // via Scrobble::check_timestamps()
                if (i > 1)
                    new_entry.when = 0;
                
                emit entry(new_entry);
            }
        }
    }

mtp_get_metadata_cleanup:
    if (album)
        CoTaskMemFree(album);
    if (artist)
        CoTaskMemFree(artist);
    if (name)
        CoTaskMemFree(name);
    if (pdp)
    {
        pdp->Release();
        pdp = NULL;
    }
    if (pdv)
    {
        pdv->Release();
        pdv = NULL;
    }
}
