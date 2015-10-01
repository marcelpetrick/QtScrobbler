/***************************************************************************
 *   Copyright (C) 2006-2009 by Tomasz Moń                                 *
 *   Copyright (C) 2009-2013 by Robert Keevil                              *
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

#include <QtGui>
#include <QCryptographicHash>
#include "settings.h"
#include "qtscrob.h"

Site_UI::Site_UI(QWidget *parent): QWidget(parent)
{
	layout = new QFormLayout(this);
	enabled = new QCheckBox(this);
	host = new QLineEdit(this);
	name = new QLineEdit(this);
	username = new QLineEdit(this);
	password = new QLineEdit(this);
	layout->addRow(tr("Enabled"), enabled);
	layout->addRow(tr("Site Name"), name);
	layout->addRow(tr("Host"), host);
	layout->addRow(tr("Username"), username);
	layout->addRow(tr("Password"), password);
}

Settings::Settings(QTScrob *parent) : QDialog( parent ) {
	setupUi(this);
	qtscrob = parent;

	//General Tab
	chkAutoOpen->setChecked(qtscrob->scrob->conf->auto_open);
	chkDelPlayList->setChecked(qtscrob->scrob->conf->del_apple_playlist);
	chkDisplayUTC->setChecked(qtscrob->scrob->conf->display_utc);

	//Sites Tab
	for (int i = 0; i < CONFIG_NUM_SITES; i++)
	{
		Site_UI *site = new Site_UI(tab_widget_sites);

		site->enabled->setChecked(qtscrob->scrob->conf->sites[i].enabled);
		site->name->setText(qtscrob->scrob->conf->sites[i].conf_name);
		site->host->setText(qtscrob->scrob->conf->sites[i].handshake_host);
		if (i != CONFIG_CUSTOM)
		{
			site->name->setEnabled(false);
			site->host->setEnabled(false);
		}
		site->username->setText(qtscrob->scrob->conf->sites[i].username);
		site->password->setText(qtscrob->scrob->conf->sites[i].password_hash);
		site->password->setEchoMode(QLineEdit::Password);
		if ( !qtscrob->scrob->conf->sites[i].password_hash.isEmpty() )
			site->password->setText("********"); // Indicate that the password has been saved

		tab_widget_sites->addTab(site, SITE_NAME[i]);
		site_tabs.push_back(site);
	}

	//Proxy Tab
	edtProxyHost->setText(qtscrob->scrob->conf->proxy_host);
	QString port;
	port.setNum(qtscrob->scrob->conf->proxy_port);
	edtProxyPort->setText(port);
	edtProxyUser->setText(qtscrob->scrob->conf->proxy_user);
	edtProxyPass->setText(qtscrob->scrob->conf->proxy_pass);

	//Timezone tab
	QString tzinfo = qtscrob->scrob->get_zonename();
	tzinfo += "<br>";
	tzinfo += tr("Offset: ");
	tzinfo += qtscrob->scrob->offset_str();
	tzinfo += "<br>";
	tzinfo += tr("Daylight saving: ");
	if (qtscrob->scrob->get_dst())
		tzinfo += tr("Yes");
	else
		tzinfo += tr("No");
	lblTZInfo->setText(tzinfo);

	cmb_tz_hour->setCurrentIndex((qtscrob->scrob->conf->utc_offset / 3600) + 12);
	cmb_tz_min->setCurrentIndex(abs(qtscrob->scrob->conf->utc_offset % 3600) / 900);
	grpManual->setChecked(qtscrob->scrob->conf->tz_override);

    connect(btnOK, SIGNAL(clicked()), this, SLOT(save()));
	connect(btnCancel, SIGNAL(clicked()), qtscrob, SLOT(settings_close()));

	tabWidget->setCurrentIndex(0);

	setFixedSize(sizeHint());
}

void Settings::save()
{
	//General Tab
	qtscrob->scrob->conf->auto_open = chkAutoOpen->isChecked();
	qtscrob->scrob->conf->del_apple_playlist = chkDelPlayList->isChecked();
	qtscrob->scrob->conf->display_utc = chkDisplayUTC->isChecked();

	//Sites Tab
	for (int i = 0; i < CONFIG_NUM_SITES; i++)
	{
		qtscrob->scrob->conf->sites[i].enabled = site_tabs.at(i)->enabled->isChecked();
		qtscrob->scrob->conf->sites[i].username = site_tabs.at(i)->username->text();
		if (i != CONFIG_CUSTOM)
		{
			qtscrob->scrob->conf->sites[i].conf_name = site_tabs.at(i)->name->text();
			qtscrob->scrob->conf->sites[i].handshake_host = site_tabs.at(i)->host->text();
		}
		if (site_tabs.at(i)->password->isModified())
		{
			QCryptographicHash pass_hash(QCryptographicHash::Md5);
			pass_hash.addData(site_tabs.at(i)->password->text().toUtf8());
			qtscrob->scrob->conf->sites[i].password_hash =
					QString(pass_hash.result().toHex());
		}
	 }

	//Proxy Tab
	qtscrob->scrob->conf->proxy_host = edtProxyHost->text();
	qtscrob->scrob->conf->proxy_port = edtProxyPort->text().toInt();
	qtscrob->scrob->conf->proxy_user = edtProxyUser->text();
	qtscrob->scrob->conf->proxy_pass = edtProxyPass->text();

	//Timezone tab
	qtscrob->scrob->conf->tz_override = grpManual->isChecked();
	int offset = (cmb_tz_hour->currentIndex()-12) * 3600;
	if (offset < 0)
		offset -= cmb_tz_min->currentIndex() * 900;
	else
		offset += cmb_tz_min->currentIndex() * 900;
	qtscrob->scrob->conf->utc_offset = offset;

	qtscrob->scrob->conf->save_config();
	qtscrob->scrob->conf->load_config();

	qtscrob->settings_close();
}
