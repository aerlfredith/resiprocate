#ifdef WIN32

#include <winsock2.h>
#include <stdlib.h>
#include <io.h>

#else

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sipstack/Data.hxx>

#endif

#include <sipstack/UdpTransport.hxx>
#include <sipstack/SipMessage.hxx>
#include <sipstack/Preparse.hxx>
#include <sipstack/Logger.hxx>

#define VOCAL_SUBSYSTEM Subsystem::SIP

using namespace std;
using namespace Vocal2;

const unsigned long
UdpTransport::MaxBufferSize = 64000;

UdpTransport::UdpTransport(int portNum, Fifo<Message>& fifo) : 
   Transport(portNum, fifo)
{
   mFd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

   if ( mFd < 0 )
   {
      //InfoLog (<< "Failed to open socket: " << portNum);
   }
   
   sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY); // !jf! 
   addr.sin_port = htons(portNum);
   
   if ( bind( mFd, (struct sockaddr*) &addr, sizeof(addr)) != 0 )
   {
      int err = errno;
      if ( err == EADDRINUSE )
      {
         //InfoLog (<< "Address already in use");
      }
      else
      {
         //InfoLog (<< "Could not bind to port: " << portNum);
      }
      
      throw TransportException("Address already in use", __FILE__,__LINE__);
   }

   // make non blocking 
#if WIN32
   unsigned long block = 0;
   int errNoBlock = ioctlsocket( mFd, FIONBIO , &block );
   assert( errNoBlock == 0 );
#else
   int flags  = fcntl( mFd, F_GETFL, 0);
   int errNoBlock = fcntl(mFd,F_SETFL, flags| O_NONBLOCK );
   assert( errNoBlock == 0 );
#endif

}

UdpTransport::~UdpTransport()
{
}

void 
UdpTransport::send( const sockaddr_in& dest,
                    const  char* buffer,
                    const size_t length) //, TransactionId txId)
{
   SendData* data = new SendData;
   data->destination = dest;
   data->buffer = buffer;
   data->length = length;
   //data->tid = txId;

   DebugLog (<< "Adding message to tx buffer: " << string(buffer, length));
   
   mTxFifo.add(data); // !jf!
}


void UdpTransport::process()
{
   // pull buffers to send out of TxFifo
   // receive datagrams from fd
   // preparse and stuff into RxFifo

   
   // how do we know that buffer won't get deleted on us !jf!
   if (mTxFifo.messageAvailable())
   {
      SendData* data = mTxFifo.getNext();
      DebugLog (<< "Sending message on udp");
      unsigned int count = ::sendto(mFd, data->buffer, data->length, 0, (const sockaddr*)&data->destination, sizeof(data->destination));
   
      if ( count < 0 )
      {
         //DebugLog (<< strerror(errno));
         // !jf! what to do if it fails
         assert(0);
      }

      assert (count == data->length || count < 0);
   }
#define UDP_SHORT   

   struct sockaddr_in from;

#if !defined(UDP_SHORT)

   // !ah! debug is just to always return a sample message

   // !jf! this may have to change - when we read a message that is too big
   char* buffer = new char[MaxBufferSize];
   int fromLen = sizeof(from);
   
   // !jf! how do we tell if it discarded bytes 
   // !ah! we use the len-1 trick :-(

   int len = recvfrom( mFd,
                       buffer,
                       MaxBufferSize,
                       0 /*flags */,
                       (struct sockaddr*)&from,
                       (socklen_t*)&fromLen);
#else

#define CRLF "\r\n"
   
   char buffer[] = 
      "INVITE foo@bar.com SIP/2.0" CRLF
      "Via: SIP/2.0/UDP pc33.atlanta.com;branch=foo" CRLF
      "To: <sip:alan@ieee.org>" CRLF
      "From: <sip:cj@whistler.com>"CRLF
      "Route: <sip:1023@1.2.3.4>, <sip:1023@3.4.5.6>, <sip:1023@10.1.1.1>" CRLF
      "Subject: Good Morning!"CRLF
      "Call-Id: 123" CRLF
      CRLF
   ;
   int len = sizeof(buffer)/sizeof(*buffer);
#endif

   if ( len <= 0 )
   {
      //int err = errno;
   }
   else if (len > 0)
   {
      //cerr << "Received : " << len << " bytes" << endl;
      
      SipMessage* message = new SipMessage;
      
      // set the received from information into the received= parameter in the
      // via
      message->addSource(from);
      message->addBuffer(buffer);

      Preparse preParser(*message,buffer,len);
      
      preParser.process();
      // this won't work if UDPs are fragd !ah!
      // save the interface information in the message
      // preparse the message
      // stuff the message in the 
      
      DebugLog (<< "adding new SipMessage to state machine's Fifo: " << message);
      mStateMachineFifo.add(message);
   }
}

