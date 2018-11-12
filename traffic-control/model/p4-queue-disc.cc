/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Stanford University
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
 * Authors: Stephen Ibanez <sibanez@stanford.edu>
 */

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/object-factory.h"
#include "ns3/socket.h"
#include "ns3/p4-pipeline.h"
#include "p4-queue-disc.h"
#include <algorithm>
#include <iterator>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("P4QueueDisc");

NS_OBJECT_ENSURE_REGISTERED (P4QueueDisc);

TypeId P4QueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::P4QueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<P4QueueDisc> ()
    .AddAttribute ( "P4file", "The P4 source file to use",
                    StringValue ("nofile"), MakeStringAccessor (&P4QueueDisc::GetP4File, &P4QueueDisc::SetP4File), MakeStringChecker ())
  ;
  return tid;
}

P4QueueDisc::P4QueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

P4QueueDisc::~P4QueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

std::string
P4QueueDisc::GetP4File (void) const
{
  NS_LOG_FUNCTION (this);
  return m_p4file;
}

void
P4QueueDisc::SetP4File (std::string p4file)
{
  NS_LOG_FUNCTION (this << p4file);
  m_p4file = pfile;

  // TODO(sibanez): initialize the P4 pipeline
}

bool
P4QueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  // create standard metadata
  std_meta_t std_meta;
  std_meta.egress_port = 0; // TODO(sibanez): set this based on the attached NetDevice or using an attribute of the P4QueueDisc class
  std_meta.egress_qdepth = GetNBytes();
  std_meta.drop = 0; 

  // perform P4 processing
  Ptr<Packet> new_packet = m_p4_pipe->process_pipeline(item->GetPacket(), std_meta);

  // replace the QueueDiscItem's packet
  item->SetPacket(new_packet);

  // drop the packet if the P4 program says to
  if (std_meta.drop)
    {
      NS_LOG_DEBUG ("Dropping packet because P4 program said to");
      DropBeforeEnqueue (item, P4_DROP);
      return false;
    }

  bool retval = GetQueueDiscClass (0)->GetQueueDisc ()->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the child queue disc
  // because QueueDisc::AddQueueDiscClass sets the drop callback

  NS_LOG_LOGIC ("Number packets in queue disc " << GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets ());

  return retval;
}

Ptr<QueueDiscItem>
P4QueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Dequeue ()) != 0)
    {
      NS_LOG_LOGIC ("Popped from qdisc: " << item);
      NS_LOG_LOGIC ("Number packets in qdisc: " << GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets ());
      return item;
    }
  
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
P4QueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  if ((item = GetQueueDiscClass (0)->GetQueueDisc ()->Peek ()) != 0)
    {
      NS_LOG_LOGIC ("Peeked from qdisc: " << item);
      NS_LOG_LOGIC ("Number packets band: " << GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets ());
      return item;
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
P4QueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("P4QueueDisc cannot have internal queues");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("P4QueueDisc cannot have any packet filters");
      return false;
    }

  if (GetNQueueDiscClasses () == 0)
    {
      // create 1 fifo queue disc
      ObjectFactory factory;
      factory.SetTypeId ("ns3::FifoQueueDisc");
      Ptr<QueueDisc> qd = factory.Create<QueueDisc> ();
      qd->Initialize ();
      Ptr<QueueDiscClass> c = CreateObject<QueueDiscClass> ();
      c->SetQueueDisc (qd);
      AddQueueDiscClass (c);
    }

  if (GetNQueueDiscClasses () != 1)
    {
      NS_LOG_ERROR ("P4QueueDisc requires exactly 1 class");
      return false;
    }

  if (m_p4file == "nofile")
    {
      NS_LOG_ERROR ("P4QueueDisc is not configured with a P4 source file");
      return false;
    }

  return true;
}

void
P4QueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3