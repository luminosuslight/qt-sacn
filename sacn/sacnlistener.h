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

#ifndef SACNLISTENER_H
#define SACNLISTENER_H

#include <QObject>
#include <QThread>
#include <vector>
#include <list>
#include <QTimer>
#include <QElapsedTimer>
#include <QPoint>
#include "streamingacn.h"
#include "sacnsocket.h"

/**
 * @brief The sACNMergedAddress struct contains the current level of a specific channel and
 * information about the sources sending to that address
 */
struct sACNMergedAddress
{
    sACNMergedAddress() {
        level = -1;
        winningSource = nullptr;
        changedSinceLastMerge = false;
    }
    /**
     * @brief level DMX value of this channel, 0-255, -1 if invalid
     */
    int level;
    /**
     * @brief winningSource is a pointer to the source with the highest priority for this address
     */
    sACNSource *winningSource;
    /**
     * @brief otherSources are pointers to other sources sending this address
     */
    QSet<sACNSource *> otherSources;
    /**
     * @brief changedSinceLastMerge is true if the value changed during last merge
     */
    bool changedSinceLastMerge;
};

typedef QList<sACNMergedAddress> sACNMergedSourceList;

/**
 * @brief The sACNListener class is used to listen to  a universe of sACN.
 * The class should not be instantiated directly; instead use sACNManager to get the
 * listener for a universe - this allows reuse of listeners
 */
class sACNListener : public QObject
{
    Q_OBJECT
public:
    sACNListener(int universe, QObject *parent = nullptr);
    virtual ~sACNListener();

    /**
     * @brief universe
     * @return the universe which this listener is listening for
     */
    int universe() {return m_universe;}
    /**
     * @brief mergedLevels
     * @return an sACNMergerdSourceList, a list of merged address structures, allowing you to see
     * the result of the merge algorithm together with all the sub-sources, by address
     */
    sACNMergedSourceList mergedLevels() { return m_merged_levels;}

    std::size_t sourceCount() { return m_sources.size();}
    sACNSource *source(std::size_t index) { return m_sources[index];}

    /**
     *  @brief processDatagram Process a suspected sACN datagram.
     * This allows other listeners to pass on unicast datagrams for other universes
     */
    void processDatagram(QByteArray data, QHostAddress receiver, QHostAddress sender);

    // Diagnostic - the number of merge operations per second

    unsigned int mergesPerSecond() { return (m_mergesPerSecond > 0) ? m_mergesPerSecond : 0;}
public slots:
    void startReception();
    void monitorAddress(int address) {
        QMutexLocker locker(&m_monitoredChannelsMutex);
        m_monitoredChannels.insert(address);
    }
    void unMonitorAddress(int address) {
        QMutexLocker locker(&m_monitoredChannelsMutex);
        m_monitoredChannels.remove(address);
    }
signals:
    void sourceFound(sACNSource *source);
    void sourceLost(sACNSource *source);
    void sourceChanged(sACNSource *source);
    void levelsChanged();
    void dataReady(int address, QPointF data);
private slots:
    void readPendingDatagrams();
    void performMerge();
    void checkSourceExpiration();
    void sampleExpiration();
private:
    std::list<sACNRxSocket *> m_sockets;
    std::vector<sACNSource *> m_sources;
    int m_last_levels[512];
    sACNMergedSourceList m_merged_levels;
    int m_universe;
    // The per-source hold last look time
    int m_ssHLL;
    // Are we in the initial sampling state
    bool m_isSampling;
    QTimer *m_initalSampleTimer;
    QTimer *m_mergeTimer;
    QElapsedTimer m_elapsedTime;
    int m_predictableTimerValue;
    QMutex m_monitoredChannelsMutex;
    QSet<int> m_monitoredChannels;
    bool m_mergeAll; // A flag to initiate a complete remerge of everything
    unsigned int m_mergesPerSecond;
    int m_mergeCounter;
    QElapsedTimer m_mergesPerSecondTimer;
};


#endif // SACNLISTENER_H
