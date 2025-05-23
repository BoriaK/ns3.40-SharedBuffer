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

#ifndef CUSTOM_TAGG_H
#define CUSTOM_TAGG_H

#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include <iostream>

using namespace ns3;


/**
 * \ingroup network
 * A Tag that represents the flows shared priority
 */
class SharedPriorityTag : public Tag
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

    // these are our accessors to our tag structure
    /**
     * Set the tag value
     * \param value The tag value.
     */
    void SetSimpleValue (uint8_t value);
    /**
     * Get the tag value
     * \return the tag value.
     */
    uint8_t GetSimpleValue (void) const;
  private:
    uint8_t m_simpleValue;  //!< tag value
};

/**
 * \ingroup network
 * A tag that represents whether this node is a Router - currently not used
 */
class SharedRouterTag : public Tag
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

    // these are our accessors to our tag structure
    /**
     * Set the tag value
     * \param isRouter The tag value.
     */
    void SetSimpleValue (bool isRouter);
    /**
     * Get the tag value
     * \return the tag value.
     */
    bool GetSimpleValue (void) const;
  private:
    bool m_isRouter;  //!< tag value
};

/**
 * \ingroup network
 * A tag that represents the mice/elephant probability (d) when flow was created
 */
class MiceElephantProbabilityTag : public Tag
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

    // these are our accessors to our tag structure
    /**
     * Set the tag value
     * \param dValue The tag value.
     */
    void SetSimpleValue (int32_t dValue);
    /**
     * Get the tag value
     * \return the tag value.
     */
    int32_t GetSimpleValue (void) const;
  private:
    int32_t m_dValue;  //!< tag value
};

/**
 * \ingroup network
 * A Tag that represents the ToS that was assigned to the OnOff application
 */
class ApplicationTosTag : public Tag
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

    // these are our accessors to our tag structure
    /**
     * Set the tag value
     * \param tosValue The tag value.
     */
    void SetTosValue (uint8_t tosValue);
    /**
     * Get the tag value
     * \return the tag value.
     */
    uint8_t GetTosValue (void) const;
  private:
    uint8_t m_tosValue;  //!< tag value
};
#endif /* CUSTOM_TAGG_H */