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