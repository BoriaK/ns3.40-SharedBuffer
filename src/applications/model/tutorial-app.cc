/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tutorial-app.h"
#include "ns3/applications-module.h"

using namespace ns3;

TutorialApp::TutorialApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    // m_threshold (10)  // Flow classfication Threshold (length), Added by me!
    m_packetsSent (0)
{
}

TutorialApp::~TutorialApp ()
{
  m_socket = 0;
}

/* static */
TypeId TutorialApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("TutorialApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<TutorialApp> ()
    .AddAttribute ("NumOfPacketsHighPrioThreshold", 
                    "The number of packets in a single sequence, up to which, "
                    "the flow will be tagged as High Priority. "
                    "If a generated flow is longer than this threshold, packets from it will be labaled as low priority",
                    UintegerValue (10),
                    MakeUintegerAccessor (&TutorialApp::m_threshold),
                    MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("FlowPriority", 
                    "The priority assigned to all the packets created by this Application, "
                    "1 = high priority, 2 = low priority"
                    "if this attribute is added, it overrides other methods to define priority",
                    UintegerValue (0),
                    MakeUintegerAccessor (&TutorialApp::m_userSetPriority),
                    MakeUintegerChecker<uint32_t> ())
    ;
  return tid;
}

void
TutorialApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
TutorialApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
TutorialApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}


void
TutorialApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  // create a tag.

  // option 1: set the priority tag arbitrarly from a user requested param:

  // option 2: set Tag value to depend on the number of previously sent packets:
  // if m_packetsSent < m_threshold: TagValue->0x0 (High Priority)
  // if m_packetsSent >= m_threshold: TagValue->0x1 (Low Priority)

  if (m_userSetPriority)
  {
    m_priority = m_userSetPriority;
  }
  else
  {
    if (m_packetsSent < m_threshold)
    {
      m_priority = 0x1;
      // flowPrioTag.SetSimpleValue (0x1);
    }
    else 
      m_priority = 0x2;
      // flowPrioTag.SetSimpleValue (0x2);
  }

  flowPrioTag.SetSimpleValue (m_priority);
  // store the tag in a packet.
  packet->AddPacketTag (flowPrioTag);
/////////////////////////

  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
TutorialApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &TutorialApp::SendPacket, this);
    }
}
