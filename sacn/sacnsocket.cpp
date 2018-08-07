// Copyright 2016 Tom Barthel-Steer
// http://www.tomsteer.net
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sacnsocket.h"

#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QDebug>
#include <QThread>
#include "ACNShare/ipaddr.h"
#include "streamcommon.h"

QNetworkInterface getDefaultNetworkInterface() {
#ifdef Q_OS_MAC
    return QNetworkInterface();
#else
    // modified from https://stackoverflow.com/a/16821776
    QList<QString> possibleMatches;
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    if (!ifaces.isEmpty()) {
        for(int i=0; i < ifaces.size(); i++) {
            unsigned int flags = ifaces[i].flags();
            bool isLoopback = bool(flags & QNetworkInterface::IsLoopBack);
            bool isP2P = bool(flags & QNetworkInterface::IsPointToPoint);
            bool isRunning = bool(flags & QNetworkInterface::IsRunning);

            // If this interface isn't running, we don't care about it
            if (!isRunning) continue;
            // We only want valid interfaces that aren't loopback/virtual and not point to point
            if (!ifaces[i].isValid() || isLoopback || isP2P) continue;

            QList<QHostAddress> addresses = ifaces[i].allAddresses();
            for(int a=0; a < addresses.size(); a++) {
                // Ignore local host
                if (addresses[a] == QHostAddress::LocalHost) continue;

                // Ignore non-ipv4 addresses
                if (!addresses[a].toIPv4Address()) continue;

                // Ignore empty addresses
                QString ip = addresses[a].toString();
                if (ip.isEmpty()) continue;

                qDebug() << "Using network interface:" << ifaces[i].humanReadableName();
                qDebug() << "IP Address:" << ip;
                return ifaces[i];
            }
        }
    }
    qCritical() << "No suitable network interface found!";
    return QNetworkInterface();
#endif
}

QNetworkInterface sACNRxSocket::s_networkInterace;

void sACNRxSocket::setNetworkInterface(QNetworkInterface iface)
{
    s_networkInterace = iface;
}

QNetworkInterface sACNTxSocket::s_networkInterace;

void sACNTxSocket::setNetworkInterface(QNetworkInterface iface)
{
    s_networkInterace = iface;
}


sACNRxSocket::sACNRxSocket(QObject *parent) : QUdpSocket(parent)
{

}

bool sACNRxSocket::bindMulticast(quint16 universe)
{
    bool ok = false;

    QNetworkInterface iface = s_networkInterace;

    CIPAddr addr;
    GetUniverseAddress(universe, addr);

    // Bind to mutlicast address
    ok = bind(addr.ToQHostAddress(),
                   addr.GetIPPort(),
                   QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);

    // Join multicast on selected NIC
    if (ok)
    {
        #if (QT_VERSION <= QT_VERSION_CHECK(5, 8, 0))
            #ifdef Q_OS_WIN
                #pragma message("setMulticastInterface() fails to bind to correct interface on systems running IPV4 and IPv6 with QT <= 5.8.0")
            #else
                #error setMulticastInterface() fails to bind to correct interface on systems running IPV4 and IPv6 with QT <= 5.8.0
            #endif
        #endif
        setMulticastInterface(iface);
        ok |= joinMulticastGroup(QHostAddress(addr.GetV4Address()), iface);
    }

    if(ok)
    {
        qDebug() << "sACNRxSocket " << QThread::currentThreadId() << ": Bound to interface:" << iface.name();
        qDebug() << "sACNRxSocket " << QThread::currentThreadId() << ": Joining Multicast Group:" << QHostAddress(addr.GetV4Address()).toString();
    }
    else
    {
        close();
        qDebug() << "sACNRxSocket " << QThread::currentThreadId() << ": Failed to bind RX socket";
    }

    return ok;
}

bool sACNRxSocket::bindUnicast()
{
    bool ok = false;

    // Bind to first IPv4 address on selected NIC
    QNetworkInterface iface = s_networkInterace;

    foreach (QNetworkAddressEntry ifaceAddr, iface.addressEntries())
    {
        if (ifaceAddr.ip().protocol() == QAbstractSocket::IPv4Protocol)
        {
            ok = bind(ifaceAddr.ip(),
                      STREAM_IP_PORT,
                      QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);
            if (ok) {
                qDebug() << "sACNRxSocket " << QThread::currentThreadId() << ": Bound to IP:" << ifaceAddr.ip().toString();
                break;
            }
        }
    }


    if (!ok) {
        close();
        qDebug() << "sACNRxSocket " << QThread::currentThreadId() << ": Failed to bind RX socket";
    }

    return ok;
}

sACNTxSocket::sACNTxSocket(QObject *parent) : QUdpSocket(parent)
{

}

bool sACNTxSocket::bindMulticast()
{
    bool ok = false;

    // Bind to first IPv4 address on selected NIC
    QNetworkInterface iface = s_networkInterace;

    foreach (QNetworkAddressEntry ifaceAddr, iface.addressEntries())
    {
        if (ifaceAddr.ip().protocol() == QAbstractSocket::IPv4Protocol)
        {
            ok = bind(ifaceAddr.ip());
            setSocketOption(QAbstractSocket::MulticastLoopbackOption, QVariant(1));
            setMulticastInterface(iface);
            qDebug() << "sACNTxSocket " << QThread::currentThreadId() << ": Bound to IP:" << ifaceAddr.ip().toString();
            break;
        }
    }
  

    if(!ok)
        qDebug() << "sACNTxSocket " << QThread::currentThreadId() << ": Failed to bind TX socket";

    return ok;
}
