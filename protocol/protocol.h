#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "../utils/array.h"
#include "findRoute.h"
#include "../utils/hashTable.h"
#include "../simulator/event.h"

extern long channelIndex, peerIndex, channelInfoIndex, paymentIndex;
extern HashTable* peers;
extern HashTable* channels;
extern HashTable* channelInfos;
extern HashTable* payments;
extern double pUncoopBeforeLock, pUncoopAfterLock;

typedef struct policy {
  double feeBase;
  double feeProportional;
  int timelock;
} Policy;

typedef struct peer {
  long ID;
  int withholdsR;
  double initialFunds;
  double remainingFunds;
  Array* channel;
} Peer;

typedef struct channelInfo {
  long ID;
  long peer1;
  long peer2;
  long channelDirection1;
  long channelDirection2;
  double capacity;
  double latency;
} ChannelInfo;

typedef struct channel {
  long ID;
  long channelInfoID;
  long counterparty;
  long otherChannelDirectionID;
  Policy policy;
  double balance;
} Channel;

typedef struct payment{
  long ID;
  long sender;
  long receiver;
  double amount;
  Route* route;
  Array* ignoredPeers;
  Array* ignoredChannels;
  int isSuccess;
  int isAPeerUncoop;
  double startTime;
  double endTime;
} Payment;


/*
typedef struct peer {
  long ID;
  Array* node;
  Array* channel;
} Peer;

typedef struct channel{
  long ID;
  long counterparty;
  double capacity;
  double balance;
  Policy policy;
} Channel;
*/

void initializeProtocolData(long nPeers, long nChannels, double pUncoopBefore, double pUncoopAfter, double RWithholding, double gini);

Peer* createPeer(long ID, long channelsSize);

ChannelInfo* createChannelInfo(long ID, long peer1, long peer2, double capacity);

Channel* createChannel(long ID, long channelInfoID, long counterparty, Policy policy);

Payment* createPayment(long ID, long sender, long receiver, double amount);

void connectPeers(long peer1, long peer2);

int isPresent(long element, Array* longArray);

double computeFee(double amountToForward, Policy policy);

void findRoute(Event* event);

void sendPayment(Event* event);

void forwardPayment(Event* event);

void receivePayment(Event* event);

void forwardSuccess(Event* event);

void receiveSuccess(Event* event);

void forwardFail(Event* event);

void receiveFail(Event* event);

long getEdgeIndex(Peer*n);

#endif
