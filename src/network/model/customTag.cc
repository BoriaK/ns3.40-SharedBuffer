/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
 *
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
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "customTag.h"
#include <iostream>

using namespace ns3;

TypeId 
SharedPriorityTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SharedPriorityTag")
    .SetParent<Tag> ()
    .AddConstructor<SharedPriorityTag> ()
    .AddAttribute ("SimpleValue",
                   "A simple value",
                   EmptyAttributeValue (),
                   MakeUintegerAccessor (&SharedPriorityTag::GetSimpleValue),
                   MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}
TypeId 
SharedPriorityTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
SharedPriorityTag::GetSerializedSize (void) const
{
  return 1;
}
void 
SharedPriorityTag::Serialize (TagBuffer i) const
{
  i.WriteU8 (m_simpleValue);
}
void 
SharedPriorityTag::Deserialize (TagBuffer i)
{
  m_simpleValue = i.ReadU8 ();
}
void 
SharedPriorityTag::Print (std::ostream &os) const
{
  os << "v=" << (uint32_t)m_simpleValue;
}
void 
SharedPriorityTag::SetSimpleValue (uint8_t value)
{
  m_simpleValue = value;
}
uint8_t 
SharedPriorityTag::GetSimpleValue (void) const
{
  return m_simpleValue;
}



TypeId 
SharedRouterTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SharedRouterTag")
    .SetParent<Tag> ()
    .AddConstructor<SharedRouterTag> ()
  ;
  return tid;
}
TypeId 
SharedRouterTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
SharedRouterTag::GetSerializedSize (void) const
{
  return 1;
}
void 
SharedRouterTag::Serialize (TagBuffer i) const
{
  i.WriteU8 (m_isRouter);
}
void 
SharedRouterTag::Deserialize (TagBuffer i)
{
  m_isRouter = i.ReadU8 ();
}
void 
SharedRouterTag::Print (std::ostream &os) const
{
  os << "v=" << (uint32_t)m_isRouter;
}
void 
SharedRouterTag::SetSimpleValue (bool isRouter)
{
  m_isRouter = isRouter;
}
bool
SharedRouterTag::GetSimpleValue (void) const
{
  return m_isRouter;
}

TypeId 
MiceElephantProbabilityTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MiceElephantProbabilityTag")
    .SetParent<Tag> ()
    .AddConstructor<MiceElephantProbabilityTag> ()
  ;
  return tid;
}
TypeId 
MiceElephantProbabilityTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
MiceElephantProbabilityTag::GetSerializedSize (void) const
{
  return 1;
}
void 
MiceElephantProbabilityTag::Serialize (TagBuffer i) const
{
  // i.WriteDouble (m_dValue);
  i.WriteU8 (m_dValue);
}
void 
MiceElephantProbabilityTag::Deserialize (TagBuffer i)
{
  // m_dValue = i.ReadDouble ();
  m_dValue = i.ReadU8 ();
}
void 
MiceElephantProbabilityTag::Print (std::ostream &os) const
{
  // os << "v=" << (double)m_dValue;
  os << "v=" << (int32_t)m_dValue;
}
void 
MiceElephantProbabilityTag::SetSimpleValue (int32_t dValue)
{
  m_dValue = dValue;
}
int32_t
MiceElephantProbabilityTag::GetSimpleValue (void) const
{
  return m_dValue;
}

TypeId 
ApplicationTosTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ApplicationTosTag")
    .SetParent<Tag> ()
    .AddConstructor<ApplicationTosTag> ()
  ;
  return tid;
}
TypeId 
ApplicationTosTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
ApplicationTosTag::GetSerializedSize (void) const
{
  return 1;
}
void 
ApplicationTosTag::Serialize (TagBuffer i) const
{
  // i.WriteDouble (m_dValue);
  i.WriteU8 (m_tosValue);
}
void 
ApplicationTosTag::Deserialize (TagBuffer i)
{
  // m_tosValue = i.ReadDouble ();
  m_tosValue = i.ReadU8 ();
}
void 
ApplicationTosTag::Print (std::ostream &os) const
{
  // os << "v=" << (double)m_tosValue;
  os << "v=" << (int32_t)m_tosValue;
}
void 
ApplicationTosTag::SetTosValue (uint8_t tosValue)
{
  m_tosValue = tosValue;
}
uint8_t
ApplicationTosTag::GetTosValue (void) const
{
  return m_tosValue;
}