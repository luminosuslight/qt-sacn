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

#include "sacnsender.h"
#include <vector>
#include <set>

#include "ACNShare/deftypes.h"
#include "ACNShare/CID.h"
#include "ACNShare/ipaddr.h"
#include "ACNShare/tock.h"
#include "streamcommon.h"

#include <QUuid>
#include <QThread>
#include <QTimer>
#include <QDebug>

sACNSentUniverse::sACNSentUniverse(unsigned short universe)
{
    m_priority = 100;
    m_name = "New Source";
    m_universe = universe;
    m_handle = 0;
    m_isSending = false;
    m_priorityMode = pmPER_SOURCE_PRIORITY;
    m_checkTimeoutTimer = Q_NULLPTR;
}

sACNSentUniverse::~sACNSentUniverse()
{
    CStreamServer *streamServer = CStreamServer::getInstance();
    streamServer->DestroyUniverse(m_handle);

    if(m_checkTimeoutTimer)
    {
        m_checkTimeoutTimer->stop();
        delete m_checkTimeoutTimer;
    }
}

void sACNSentUniverse::startSending(bool preview)
{
    uint1 options = 0;

    CStreamServer *streamServer = CStreamServer::getInstance();
    if(m_cid.isNull())
        m_cid = CID::CreateCid();

    if (preview)
        options += PREVIEW_DATA_OPTION;

    if(m_unicastAddress.isNull())
        streamServer->CreateUniverse(m_cid, qPrintable(m_name), m_priority, 0, options, 0,
           m_universe, 512, m_slotData, m_handle, false, 850, CIPAddr(), m_version==StreamingACNProtocolVersion::sACNProtocolDraft );
    else
        streamServer->CreateUniverse(m_cid, qPrintable(m_name), m_priority, 0, options, 0, m_universe,
             512, m_slotData, m_handle, false, 850, CIPAddr(m_unicastAddress), m_version==StreamingACNProtocolVersion::sACNProtocolDraft );

    streamServer->SetUniverseDirty(m_handle);

    if(m_priorityMode == pmPER_ADDRESS_PRIORITY)
    {
        uint1 *pslots;
        streamServer->CreateUniverse(m_cid, qPrintable(m_name), 0, options, 0, 0xDD, m_universe, 512, pslots, m_priorityHandle);
        memcpy(pslots, m_perChannelPriorities, sizeof(m_perChannelPriorities));
        streamServer->SetUniverseDirty(m_priorityHandle);
    }
    m_isSending = true;

    int numSecondsOfSacn = 0;  // TODO: this should be configurable
    int seconds = numSecondsOfSacn;
    if(seconds>0)
    {
        m_checkTimeoutTimer = new QTimer(this);
        connect(m_checkTimeoutTimer, SIGNAL(timeout()), this, SLOT(doTimeout()));
        m_checkTimeoutTimer->setSingleShot(true);
        m_checkTimeoutTimer->setInterval(1000 * seconds);
        m_checkTimeoutTimer->start();
    }
}


void sACNSentUniverse::stopSending()
{
    CStreamServer::getInstance()->DestroyUniverse(m_handle);
    m_handle = 0;
    m_isSending = false;

    if(m_priorityMode == pmPER_ADDRESS_PRIORITY)
    {
        CStreamServer::getInstance()->DestroyUniverse(m_priorityHandle);
        m_priorityHandle = 0;
    }
}

void sACNSentUniverse::setLevel(quint16 address, quint8 value)
{
    Q_ASSERT(address<512);
    if(isSending())
    {
        m_slotData[address] =  value;
        CStreamServer::getInstance()->SetUniverseDirty(m_handle);
    }
}

void sACNSentUniverse::setLevelRange(quint16 start, quint16 end, quint8 value)
{
    Q_ASSERT(start<512);
    Q_ASSERT(end<512);
    Q_ASSERT(start<=end);
    if(isSending())
    {
        memset(m_slotData + start, value, end-start+1);
        CStreamServer::getInstance()->SetUniverseDirty(m_handle);
    }
}

void sACNSentUniverse::setLevel(const quint8 *data, int len, int start)
{
    if(isSending())
    {
        memcpy(m_slotData + start, data, len);
        CStreamServer::getInstance()->SetUniverseDirty(m_handle);
    }
}

void sACNSentUniverse::setVerticalBar(quint16 index, quint8 level)
{
    Q_ASSERT(index<32);

    if(isSending())
    {
        memset(m_slotData, 0, 512);
        for(int i=0; i<16; i++)
        {
            m_slotData[(i*32)+index] = level;
        }

        CStreamServer::getInstance()->SetUniverseDirty(m_handle);
    }
}


void sACNSentUniverse::setHorizontalBar(quint16 index, quint8 level)
{
    Q_ASSERT(index<16);

    if(isSending())
    {
        memset(m_slotData, 0, 512);
        memset(m_slotData + 32*index, level, 32);

        CStreamServer::getInstance()->SetUniverseDirty(m_handle);
    }
}

void sACNSentUniverse::setName(const QString &name)
{
    m_name = name;
    if(isSending())
    {
        QByteArray arr = name.toUtf8();
        CStreamServer::getInstance()->setUniverseName(m_handle ,arr.constData());
    }
}

void sACNSentUniverse::setPriorityMode(PriorityMode mode)
{
    m_priorityMode = mode;
}

void sACNSentUniverse::setPerChannelPriorities(uint1 *priorities)
{
    memcpy(m_perChannelPriorities, priorities, sizeof(m_perChannelPriorities));
}

void sACNSentUniverse::setPerSourcePriority(uint1 priority)
{
    m_priority = priority;
    if(m_isSending)
    {
         CStreamServer::getInstance()->setUniversePriority(m_handle, priority);
    }
}

void sACNSentUniverse::setProtocolVersion(StreamingACNProtocolVersion version)
{
    m_version = version;
}

void sACNSentUniverse::copyLevels(quint8 *dest)
{
    memcpy(dest, m_slotData, MAX_DMX_ADDRESS);
}

void sACNSentUniverse::setUniverse(int universe)
{
    if(m_universe==universe) return;
    m_universe = universe;
    if(m_isSending)
    {
        stopSending();
        startSending();
    }
}

void sACNSentUniverse::doTimeout()
{
    delete m_checkTimeoutTimer;
    m_checkTimeoutTimer = 0;
    stopSending();
    emit sendingTimeout();
    qDebug() << "Source " << this->name() << " timeout";
}

CStreamServer *CStreamServer::m_instance = 0;

CStreamServer *CStreamServer::getInstance()
{
    if(!m_instance)
        m_instance = new CStreamServer();
    return m_instance;
}

void CStreamServer::shutdown()
{
    if(m_instance)
        m_instance->deleteLater();
    m_instance = Q_NULLPTR;
}

CStreamServer::CStreamServer()
{
    m_sendsock = new sACNTxSocket();

    m_sendsock->bindMulticast();

    m_thread = new QThread();
    connect(m_thread, &QThread::finished, this, &QObject::deleteLater);
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(10);
    connect(m_tickTimer, SIGNAL(timeout()), this, SLOT(Tick()), Qt::DirectConnection);
    m_tickTimer->start();
    this->moveToThread(m_thread);
    m_thread->start();
}

CStreamServer::~CStreamServer()
{
    //Clean up the sequence numbers
    for(seqiter it3 = m_seqmap.begin(); it3 != m_seqmap.end(); ++it3)
    {
        if(it3->second.second)
        {
            delete it3->second.second;
            it3->second.second = Q_NULLPTR;
        }
    }
    m_thread->quit();
}


//Returns a pointer to the storage location for the universe, adding if
//need be.
//The newly-added location contains sequence number 0.
uint1* CStreamServer::GetPSeq(const CID &cid, uint2 universe)
{
    cidanduniverse identifier(cid, universe);

    seqiter it = m_seqmap.find(identifier);
    if(it != m_seqmap.end())
    {
        ++it->second.first;
        return it->second.second;
    }
    uint1 * p = new uint1;
    if(!p)
        return NULL;
    *p = 0;
    m_seqmap.insert(std::pair<cidanduniverse, seqref>(identifier, seqref(1, p)));
    return p;
}

//Removes a reference to the storage location for the universe, removing
//completely if need be.
void CStreamServer::RemovePSeq(const CID &cid, uint2 universe)
{
    cidanduniverse identifier(cid, universe);

    seqiter it = m_seqmap.find(identifier);

    if(it != m_seqmap.end())
    {
        --it->second.first;
        if(it->second.first <= 0)
        {
            delete it->second.second;
            m_seqmap.erase(it);
        }
    }
}


void CStreamServer::Tick()
{
    QMutexLocker locker(&m_writeMutex);

    int valid_count = 0;
    for(verseiter it = m_multiverse.begin(); it != m_multiverse.end(); ++it)
    {
        if(it->psend)
            ++valid_count;

        //If this has been send 3 times (or more?) with a termination flag
        //then it's time to kill it
        if(it->num_terminates >= 3)
        {
            DoDestruction(it->handle);
        }

        //If valid, either a dirty, inactivity count < 3 (if we're using that logic), or send_interval will cause a send
        if(it->psend && (it->isdirty ||
                             (it->waited_for_dirty &&
                                ((!it->ignore_inactivity && it->inactive_count < 3) ||
                                 it->send_interval.Expired()))))
        {
            //Before the send, properly reset state
            if(it->isdirty)
                it->inactive_count = 0;  //To recover from inactivity
            else if(it->inactive_count < 3)  //We don't want the Expired case to reset the inactivity count
                ++it->inactive_count;

            //Add the sequence number and send
            uint1 *pseq = GetPSeq(it->cid, it->number);
            SetStreamHeaderSequence(it->psend, *pseq, it->draft);
            (*pseq)++;

            quint64 result = m_sendsock->writeDatagram( (char*)it->psend, it->sendsize, it->sendaddr, STREAM_IP_PORT);
            if(result!=it->sendsize)
            {
                qDebug() << "Error sending datagram : " << m_sendsock->errorString();
            }

            if(GetStreamTerminated(it->psend))
            {
                it->num_terminates++;
            }

            //Finally, set the timing/dirtiness for the next interval
            it->isdirty = false;
            it->send_interval.Reset();
        }
    }

}


//Use this to create a universe for a source cid, startcode, etc.
//If it returns true, two parameters are filled in: The data buffer for the values that can
//  be manipulated directly, and the handle to use when calling the rest of these functions.
//Note that a universe number of 0 is invalid.
//Set reserved to be 0 and options to either be 0 or PREVIEW_DATA_OPTION (can be changed later
//  by OptionsPreviewData).
//if ignore_inactivity_logic is set to false (the default for DMX), Tick will handle sending the
//  3 identical packets at the lower frequency that sACN requires.  It sends these packets at
//  send_intervalms intervals (again defaulted for DMX).  Note that even if you are not using the
//  inactivity logic, send_intervalms expiry will trigger a resend of the current universe packet.
//Data on this universe will not be initially sent until marked dirty.
bool CStreamServer::CreateUniverse(const CID& source_cid, const char* source_name, uint1 priority, uint2 reserved, uint1 options, uint1 start_code,
                                     uint2 universe, uint2 slot_count, uint1*& pslots, uint& handle,
                                        bool ignore_inactivity_logic, uint send_intervalms, CIPAddr unicastAddress, bool draft)
{
    QMutexLocker locker(&m_writeMutex);
    if(universe == 0)
        return false;

   //Before we attempt to create the universe, make sure we can create the buffer.
    uint sendsize = slot_count;
    if(draft)
        sendsize += DRAFT_STREAM_HEADER_SIZE;
    else
        sendsize += STREAM_HEADER_SIZE;

    uint1* pbuf = new uint1 [sendsize];
    if(!pbuf)
        return false;
    memset(pbuf, 0, sendsize);

    // Find a slot if we can
    bool foundSpace = false;
    for(unsigned int i=0; i<m_multiverse.size(); i++)
    {
        if(m_multiverse[i].psend == NULL)
        {
            handle = i;
            foundSpace = true;
            break;
        }
    }

    if(!foundSpace)
    {
        // Allocate a new universe
        struct universe tmp;
        handle = m_multiverse.size();
        m_multiverse.push_back(tmp);
    }

    //Init the state
    m_multiverse[handle].number = universe;
    m_multiverse[handle].handle = handle;
    m_multiverse[handle].start_code = start_code;
    m_multiverse[handle].isdirty = false;
    m_multiverse[handle].waited_for_dirty = false;
    m_multiverse[handle].num_terminates=0;
    m_multiverse[handle].ignore_inactivity = ignore_inactivity_logic;
    m_multiverse[handle].inactive_count = 0;
    m_multiverse[handle].send_interval.SetInterval(send_intervalms);
    m_multiverse[handle].draft = draft;
    m_multiverse[handle].cid = source_cid;

    CIPAddr addr;
    GetUniverseAddress(universe, addr);

    if(unicastAddress != CIPAddr())
    {
        addr = unicastAddress;
    }

    m_multiverse[handle].sendaddr = addr.ToQHostAddress();

    if(draft)
        InitStreamHeaderForDraft(pbuf, source_cid, source_name, priority, reserved, options, start_code, universe, slot_count);
    else
        InitStreamHeader(pbuf, source_cid, source_name, priority, reserved, options, start_code, universe, slot_count);

    m_multiverse[handle].psend = pbuf;
    m_multiverse[handle].sendsize = sendsize;
    if(draft)
        pslots = pbuf + DRAFT_STREAM_HEADER_SIZE;
    else
        pslots = pbuf + STREAM_HEADER_SIZE;
    return true;
}

//After you add data to the data buffer, call this to trigger the data send on
//the next Tick boundary.
//Otherwise, the data won't be sent until the inactivity or send_interval time.
void CStreamServer::SetUniverseDirty(uint handle)
{
    QMutexLocker locker(&m_writeMutex);
    m_multiverse[handle].isdirty = true;
    m_multiverse[handle].waited_for_dirty = true;
}

//In the event that you want to send out a message for a particular
//universe (and start code) in between ticks, call this function.
//This does not affect the dirty bit for the universe, inactivity count,
//etc, and the tick will still operate normally when called.
//This is not thread safe with Tick -- Don't call when Tick is called
void CStreamServer::SendUniverseNow(uint handle)
{
    //Basically, a copy of the sending part of Tick

    universe* puni = &m_multiverse[handle];
    uint1 *pseq = GetPSeq(puni->cid, puni->number);
    SetStreamHeaderSequence(puni->psend, *pseq, puni->draft);
    (*pseq)++;

    m_sendsock->writeDatagram((char*) puni->psend, puni->sendsize, puni->sendaddr, STREAM_IP_PORT);
}

//Use this to destroy a priority universe.
void CStreamServer::DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle)
{
  DoDestruction(handle);
}

//Use this to destroy a universe.
//this does invalidate the pslots array that CreateUniverse returned, so do not access
//that memory after or during this call.
//Not Thread Safe -- Don't call when Tick is called
void CStreamServer::DestroyUniverse(uint handle)
{
    QMutexLocker locker(&m_writeMutex);
    if(handle < m_multiverse.size())
        SetStreamTerminated(m_multiverse[handle].psend, true);
}

//Perform the logical destruction and cleanup of a universe and its related
//objects.
void CStreamServer::DoDestruction(uint handle)
{
  if(m_multiverse[handle].psend)
    {
      m_multiverse[handle].num_terminates = 0;
      delete [] m_multiverse[handle].psend;
      m_multiverse[handle].psend = NULL;
    }
}


//sets the preview_data bit of the options field
void CStreamServer::OptionsPreviewData(uint handle, bool preview)
{
  SetPreviewData(m_multiverse[handle].psend, preview);
}

//sets the stream_terminated bit of the options field
void CStreamServer::OptionsStreamTerminated(uint handle, bool terminated)
{
  SetStreamTerminated(m_multiverse[handle].psend, terminated);
}

void CStreamServer::setUniverseName(uint handle, const char *name)
{
    if(m_multiverse[handle].psend)
    {
        strncpy((char *)m_multiverse[handle].psend + SOURCE_NAME_ADDR,
                name,
                DRAFT_SOURCE_NAME_SIZE);
    }
}


void CStreamServer::setUniversePriority(uint handle, uint1 priority)
{
    if(m_multiverse[handle].psend && m_multiverse[handle].draft)
    {
        m_multiverse[handle].psend[DRAFT_PRIORITY_ADDR] = priority;
    }
    else if(m_multiverse[handle].psend)
    {
        m_multiverse[handle].psend[PRIORITY_ADDR] = priority;
    }
}

