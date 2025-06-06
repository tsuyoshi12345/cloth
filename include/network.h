#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "cloth.h"
#include "list.h"


#define MAXMSATOSHI 5E17 //5 millions  bitcoin
#define MAXTIMELOCK 100
#define MINTIMELOCK 1
#define MAXFEEBASE 5000
#define MINFEEBASE 1000
#define MAXFEEPROP 10
#define MINFEEPROP 1
#define MAXLATENCY 100
#define MINLATENCY 10
#define MINBALANCE 1E2
#define MAXBALANCE 1E11

//ノードのタイプを決める
enum node_type {
  LND,
  CLN,
  NO_TYPE
};

/* a policy that must be respected when forwarding a payment through an edge (see edge below) */
struct policy {
  uint64_t fee_base;
  uint64_t fee_proportional;
  uint64_t min_htlc;
  uint32_t timelock;
};

/* a node of the payment-channel network */
struct node {
  long id;
  enum node_type type;
  struct array* open_edges;
  struct element **results;
  unsigned int explored;
  struct array* open_leases;
  struct lease_rates* rates;
  uint32_t funding_feerate_perkw;
  double lease_threshold;
  bool lease_provider;
  uint64_t available_funds;
  struct funder_policy* funder_policy;
};

/* a bidirectional payment channel of the payment-channel network open between two nodes */
struct channel {
  long id;
  long node1;
  long node2;
  long edge1;
  long edge2;
  uint64_t capacity;
  unsigned int is_closed;
  //add lease_expiry
  long lease_expiry;
  bool is_leased;
};

/* an edge represents one of the two direction of a payment channel */
struct edge {
  long id;
  long channel_id;
  long from_node_id;
  long to_node_id;
  long counter_edge_id;
  struct policy policy;
  uint64_t balance;
  unsigned int is_closed;
  uint64_t tot_flows;
};


struct graph_channel {
  long node1_id;
  long node2_id;
};


struct network {
  struct array* nodes;
  struct array* channels;
  struct array* edges;
  long block_height;
  gsl_ran_discrete_t* faulty_node_prob; //the probability that a nodes in the network has a fault and goes offline
};


struct node* new_node(long id, enum node_type type,uint16_t lease_fee_basis,uint32_t lease_fee_base_sat,uint16_t channel_fee_max_proportional_thousandths,
uint32_t channel_fee_max_base_msat,uint16_t funding_weight,uint32_t funding_feerate_perkw,double lease_threshold,uint64_t available_funds, struct funder_policy* policy);

struct channel* new_channel(long id, long direction1, long direction2, long node1, long node2, uint64_t capacity);

struct edge* new_edge(long id, long channel_id, long counter_edge_id, long from_node_id, long to_node_id, uint64_t balance, struct policy policy);

void open_lease_channel(struct network* network, struct node* from_node, struct edge* edge, gsl_rng* random_generator);

struct network* initialize_network(struct network_params net_params, gsl_rng* random_generator);

double get_balance_rate(struct edge* edge, struct network* network);

uint64_t get_request_amt(struct edge* edge, struct network* network);

bool check_liquidity_node(struct node* node, struct network* network);

void check_liquidity_network(struct network* network);

bool check_liquidity(struct edge* edge, struct network* network, double lease_threshold);

#endif
