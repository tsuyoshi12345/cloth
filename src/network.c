#include <string.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_rng.h>
#include "../include/network.h"
#include "../include/array.h"

#include "../include/lease.h"
#include "../include/htlc.h"
/* Functions in this file generate a payment-channel network where to simulate the execution of payments */

struct node* new_node(long id, enum node_type type,uint16_t lease_fee_basis,uint32_t lease_fee_base_sat,uint16_t channel_fee_max_proportional_thousandths,
uint32_t channel_fee_max_base_msat,uint16_t funding_weight,uint32_t funding_feerate_perkw,double lease_threshold,uint64_t available_funds, struct funder_policy* policy) {
  struct node* node;
  node = malloc(sizeof(struct node));
  node->id=id;
  node->type = type;
  node->open_edges = array_initialize(10);
  node->results = NULL;
  node->explored = 0;
  if(node->type == 1){
    node->open_leases = array_initialize(10);
    node->rates = lease_rates_initialize(lease_fee_basis, lease_fee_base_sat, channel_fee_max_proportional_thousandths, channel_fee_max_base_msat, funding_weight);
    node->funding_feerate_perkw = funding_feerate_perkw;
    node->lease_threshold = lease_threshold;
    node->available_funds = available_funds;
    node->lease_provider = true;
    node->funder_policy = policy;
  }else{
    node->open_leases = NULL;
    node->rates = NULL;
    node->lease_provider = false;
  }
  return node;
  
}

struct channel* new_channel(long id, long direction1, long direction2, long node1, long node2, uint64_t capacity) {
  struct channel* channel;
  channel = malloc(sizeof(struct channel));
  channel->id = id;
  channel->edge1 = direction1;
  channel->edge2 = direction2;
  channel->node1 = node1;
  channel->node2 = node2;
  channel->capacity = capacity;
  channel->is_closed = 0;
  channel->is_leased = false;
  channel->lease_expiry = 0;
  return channel;
}

struct channel* new_lease_channel(long id, long direction1, long direction2, long node1, long node2, uint64_t capacity, long lease_expiry) {
  struct channel* channel;
  channel = malloc(sizeof(struct channel));
  channel->id = id;
  channel->edge1 = direction1;
  channel->edge2 = direction2;
  channel->node1 = node1;
  channel->node2 = node2;
  channel->capacity = capacity;
  channel->is_closed = 0;
  channel->is_leased = true;
  channel->lease_expiry = lease_expiry;
  return channel;
}

struct edge* new_edge(long id, long channel_id, long counter_edge_id, long from_node_id, long to_node_id, uint64_t balance, struct policy policy){
  struct edge* edge;
  edge = malloc(sizeof(struct edge));
  edge->id = id;
  edge->channel_id = channel_id;
  edge->from_node_id = from_node_id;
  edge->to_node_id = to_node_id;
  edge->counter_edge_id = counter_edge_id;
  edge->policy = policy;
  edge->balance = balance;
  edge->is_closed = 0;
  edge->tot_flows = 0;
  return edge;
}

enum node_type compare_node_type(char* node_type_name){
  if (strcmp(node_type_name, "LND") == 0) return LND;
  if(strcmp(node_type_name, "CLN") == 0) return CLN;

  return NO_TYPE;
}


/* after generating a network, write it in csv files "nodes.csv" "edges.csv" "channels.csv" */
void write_network_files(struct network* network){
  FILE* nodes_output_file, *edges_output_file, *channels_output_file;
  long i;
  struct node* node;
  struct channel* channel;
  struct edge* edge;

  nodes_output_file = fopen("nodes.csv", "w");
  if(nodes_output_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "nodes.csv");
    exit(-1);
  }
  fprintf(nodes_output_file, "id\n");
  channels_output_file = fopen("channels.csv", "w");
  if(channels_output_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "channels.csv");
    fclose(nodes_output_file);
    exit(-1);
  }
  fprintf(channels_output_file, "id,edge1_id,edge2_id,node1_id,node2_id,capacity\n");
  edges_output_file = fopen("edges.csv", "w");
  if(edges_output_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "edges.csv");
    fclose(nodes_output_file);
    fclose(channels_output_file);
    exit(-1);
  }
  fprintf(edges_output_file, "id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock\n");

  for(i=0; i<array_len(network->nodes); i++){
    node = array_get(network->nodes, i);
    fprintf(nodes_output_file, "%ld\n", node->id);
  }

  for(i=0; i<array_len(network->channels); i++){
    channel = array_get(network->channels, i);
    fprintf(channels_output_file, "%ld,%ld,%ld,%ld,%ld,%ld\n", channel->id, channel->edge1, channel->edge2, channel->node1, channel->node2, channel->capacity);
  }

  for(i=0; i<array_len(network->edges); i++){
    edge = array_get(network->edges, i);
    fprintf(edges_output_file, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d\n", edge->id, edge->channel_id, edge->counter_edge_id, edge->from_node_id, edge->to_node_id, edge->balance, (edge->policy).fee_base, (edge->policy).fee_proportional, (edge->policy).min_htlc, (edge->policy).timelock);
  }

  fclose(nodes_output_file);
  fclose(edges_output_file);
  fclose(channels_output_file);
}


void update_probability_per_node(double *probability_per_node, int *channels_per_node, long n_nodes, long node1_id, long node2_id, long tot_channels){
  long i;
  channels_per_node[node1_id] += 1;
  channels_per_node[node2_id] += 1;
  for(i=0; i<n_nodes; i++)
    probability_per_node[i] = ((double)channels_per_node[i])/tot_channels;
}

/*リースするときのチャネルを作成*/
void lease_channel(struct channel channel_data, struct amount_sat our_funding, struct amount_sat requested_sats, struct network* network, gsl_rng*random_generator) {
  uint64_t capacity, edge1_balance, edge2_balance;
  struct policy edge1_policy, edge2_policy;
  double min_htlcP[]={0.7, 0.2, 0.05, 0.05}, fraction_capacity;
  gsl_ran_discrete_t* min_htlc_discrete;
  struct channel* channel;
  struct edge* edge1, *edge2;
  struct node* node;
  struct amount_sat lease_fee;
  struct amount_sat contribute_amount;
  struct amount_sat their_funding;
  long node_id;
  long lease_expiry; //リース期間（ブロック数）

  //leaseするノード情報を取得し、手数料を計算
  node = array_get(network->nodes, channel_data.node2);
  if(!calculate_our_funding(node->funder_policy,our_funding, amount_sat(node->available_funds), requested_sats, &their_funding)){
    return;
  }
  if(!lease_rates_calc_fee(node->rates, requested_sats, requested_sats, node->funding_feerate_perkw, &lease_fee)){
    //fprintf(stderr,"手数料の計算に失敗しました");
    return;
  }
  printf("\nlease_openning\n");
  printf("our_funding: %lld\n", our_funding.satoshis);
  printf("their_funding: %lld\n", their_funding.satoshis);
  printf("lease_fee: %lld\n", lease_fee.satoshis);

  capacity = their_funding.satoshis + our_funding.satoshis + lease_fee.satoshis;
  lease_expiry = network->block_height + 4032;
  //lease_expiry = network->block_height + 32;
  channel = new_lease_channel(channel_data.id, channel_data.edge1, channel_data.edge2, channel_data.node1, channel_data.node2, capacity, lease_expiry);

  if (our_funding.satoshis + lease_fee.satoshis >= their_funding.satoshis){
    fprintf(stderr, "手数料が高すぎます\n");
    return;
  }
  
  edge1_balance = our_funding.satoshis;
  
  if (their_funding.satoshis + lease_fee.satoshis < their_funding.satoshis){
    fprintf(stderr, "オーバフローを起こしました\n");
    return;
  }
  edge2_balance = their_funding.satoshis + lease_fee.satoshis;
  //multiplied by 1000 to convert satoshi to millisatoshi
  //edge1_balance*=1000;
  //edge2_balance*=1000;

  min_htlc_discrete = gsl_ran_discrete_preproc(4, min_htlcP);
  edge1_policy.fee_base = gsl_rng_uniform_int(random_generator, MAXFEEBASE - MINFEEBASE) + MINFEEBASE;
  edge1_policy.fee_proportional = (gsl_rng_uniform_int(random_generator, MAXFEEPROP-MINFEEPROP)+MINFEEPROP);
  edge1_policy.timelock = gsl_rng_uniform_int(random_generator, MAXTIMELOCK-MINTIMELOCK)+MINTIMELOCK;
  edge1_policy.min_htlc = gsl_pow_int(10, gsl_ran_discrete(random_generator, min_htlc_discrete));
  edge1_policy.min_htlc = edge1_policy.min_htlc == 1 ? 0 : edge1_policy.min_htlc;
  edge2_policy.fee_base = gsl_rng_uniform_int(random_generator, MAXFEEBASE - MINFEEBASE) + MINFEEBASE;
  edge2_policy.fee_proportional = (gsl_rng_uniform_int(random_generator, MAXFEEPROP-MINFEEPROP)+MINFEEPROP);
  edge2_policy.timelock = gsl_rng_uniform_int(random_generator, MAXTIMELOCK-MINTIMELOCK)+MINTIMELOCK;
  edge2_policy.min_htlc = gsl_pow_int(10, gsl_ran_discrete(random_generator, min_htlc_discrete));
  edge2_policy.min_htlc = edge2_policy.min_htlc == 1 ? 0 : edge2_policy.min_htlc;

  edge1 = new_edge(channel_data.edge1, channel_data.id, channel_data.edge2, channel_data.node1, channel_data.node2, edge1_balance, edge1_policy);
  edge2 = new_edge(channel_data.edge2, channel_data.id, channel_data.edge1, channel_data.node2, channel_data.node1, edge2_balance, edge2_policy);

  network->channels = array_insert(network->channels, channel);
  network->edges = array_insert(network->edges, edge1);
  network->edges = array_insert(network->edges, edge2);

  node = array_get(network->nodes, channel_data.node1);
  node->open_edges = array_insert(node->open_edges, &(edge1->id));
  node->open_leases = array_insert(node->open_leases, &(channel->id));
  node->available_funds -= our_funding.satoshis + lease_fee.satoshis;

  printf("opener: %ld\n",channel->node1);
  printf("balance: %ld\n", edge1->balance);
  printf("available_funds: %ld\n", node->available_funds);

  node = array_get(network->nodes, channel_data.node2);
  node->open_edges = array_insert(node->open_edges, &(edge2->id));
  node->open_leases = array_insert(node->open_leases, &(channel->id));
  printf("チャネル開設前 leaser->available_funds: %ld\n", node->available_funds);
  node->available_funds -= their_funding.satoshis;

  printf("leaser: %ld\n",channel->node2);
  printf("balance: %ld\n", edge2->balance);
  printf("available_funds: %ld\n", node->available_funds);
  printf("channel open successful\n");
  printf("---------------------\n\n");

}

//リースネットワーク
/*void lease_offer(struct network* network){
  int i;
  struct node* node;

  network->lease_nodes = array_initialize(1000);

  for(i=0; i<array_len(network->nodes); i++){
    node = array_get(network->nodes, i);
    if(node->type == 1){
      network->nodes = array_insert(network->nodes, node);
    }
  }
}*/


/* generate a channel (connecting node1_id and node2_id) with random values */ 
void generate_random_channel(struct channel channel_data, uint64_t mean_channel_capacity, struct network* network, gsl_rng*random_generator) {
  uint64_t capacity, edge1_balance, edge2_balance;
  struct policy edge1_policy, edge2_policy;
  double min_htlcP[]={0.7, 0.2, 0.05, 0.05}, fraction_capacity;
  gsl_ran_discrete_t* min_htlc_discrete;
  struct channel* channel;
  struct edge* edge1, *edge2;
  struct node* node;

  capacity = fabs(mean_channel_capacity + gsl_ran_ugaussian(random_generator)); 
  channel = new_channel(channel_data.id, channel_data.edge1, channel_data.edge2, channel_data.node1, channel_data.node2, capacity*1000);

  fraction_capacity = gsl_rng_uniform(random_generator);
  edge1_balance = fraction_capacity*((double) capacity);
  edge2_balance = capacity - edge1_balance;
  //multiplied by 1000 to convert satoshi to millisatoshi
  edge1_balance*=1000;
  edge2_balance*=1000;

  min_htlc_discrete = gsl_ran_discrete_preproc(4, min_htlcP);
  edge1_policy.fee_base = gsl_rng_uniform_int(random_generator, MAXFEEBASE - MINFEEBASE) + MINFEEBASE;
  edge1_policy.fee_proportional = (gsl_rng_uniform_int(random_generator, MAXFEEPROP-MINFEEPROP)+MINFEEPROP);
  edge1_policy.timelock = gsl_rng_uniform_int(random_generator, MAXTIMELOCK-MINTIMELOCK)+MINTIMELOCK;
  edge1_policy.min_htlc = gsl_pow_int(10, gsl_ran_discrete(random_generator, min_htlc_discrete));
  edge1_policy.min_htlc = edge1_policy.min_htlc == 1 ? 0 : edge1_policy.min_htlc;
  edge2_policy.fee_base = gsl_rng_uniform_int(random_generator, MAXFEEBASE - MINFEEBASE) + MINFEEBASE;
  edge2_policy.fee_proportional = (gsl_rng_uniform_int(random_generator, MAXFEEPROP-MINFEEPROP)+MINFEEPROP);
  edge2_policy.timelock = gsl_rng_uniform_int(random_generator, MAXTIMELOCK-MINTIMELOCK)+MINTIMELOCK;
  edge2_policy.min_htlc = gsl_pow_int(10, gsl_ran_discrete(random_generator, min_htlc_discrete));
  edge2_policy.min_htlc = edge2_policy.min_htlc == 1 ? 0 : edge2_policy.min_htlc;

  edge1 = new_edge(channel_data.edge1, channel_data.id, channel_data.edge2, channel_data.node1, channel_data.node2, edge1_balance, edge1_policy);
  edge2 = new_edge(channel_data.edge2, channel_data.id, channel_data.edge1, channel_data.node2, channel_data.node1, edge2_balance, edge2_policy);

  network->channels = array_insert(network->channels, channel);
  network->edges = array_insert(network->edges, edge1);
  network->edges = array_insert(network->edges, edge2);

  node = array_get(network->nodes, channel_data.node1);
  node->open_edges = array_insert(node->open_edges, &(edge1->id));
  node = array_get(network->nodes, channel_data.node2);
  node->open_edges = array_insert(node->open_edges, &(edge2->id));
}


/* generate a random payment-channel network;
   the model of the network is a snapshot of the Lightning Network (files "nodes_ln.csv", "channels_ln.csv");
   starting from this network, a random network is generated using the scale-free network model */
struct network* generate_random_network(struct network_params net_params, gsl_rng* random_generator){
  FILE* nodes_input_file, *channels_input_file;
  char row[256], node_type_name[256];
  enum node_type node_type;
  long node_id_counter=0, id, channel_id_counter=0, tot_nodes, i, tot_channels, node_to_connect_id, edge_id_counter=0, j;
  double *probability_per_node;
  int *channels_per_node;
  struct network* network;
  struct node* node;
  gsl_ran_discrete_t* connection_probability;
  struct channel channel;
  uint16_t funding_weight; 
  uint16_t lease_fee_basis; 
  uint16_t channel_fee_max_proportional_thousandths; 
	uint32_t lease_fee_base_sat; 
  uint32_t channel_fee_max_base_msat;
  uint32_t funding_feerate_perkw;
  double lease_threshold;
  uint64_t availabale_funds;
  uint64_t min_their_funding_msat;
  uint64_t max_their_funding_msat;
  uint64_t per_channel_min;
  uint64_t per_channel_max;
  struct funder_policy* funder_policy;

  nodes_input_file = fopen("nodes_ln.csv", "r");
  if(nodes_input_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "nodes_ln.csv");
    exit(-1);
  }
  channels_input_file = fopen("channels_ln.csv", "r");
  if(channels_input_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "channels_ln.csv");
    fclose(nodes_input_file);
    exit(-1);
  }

  network = (struct network*) malloc(sizeof(struct network));
  network->nodes = array_initialize(1000);
  network->channels = array_initialize(1000);
  network->edges = array_initialize(2000);

  fgets(row, 256, nodes_input_file);
  while(fgets(row, 256, nodes_input_file)!=NULL) {
    sscanf(row, "%ld,%*d,%s", &id, node_type_name);
    node_type = compare_node_type(node_type_name);
    node = new_node(id, node_type,lease_fee_basis, lease_fee_base_sat,channel_fee_max_proportional_thousandths,channel_fee_max_base_msat,funding_weight,funding_feerate_perkw,lease_threshold,
      availabale_funds, funder_policy);
    network->nodes = array_insert(network->nodes, node);
    node_id_counter++;
  }
  tot_nodes = node_id_counter + net_params.n_nodes;
  if(tot_nodes == 0){
    fprintf(stderr, "ERROR: it is not possible to generate a network with 0 nodes\n");
    fclose(nodes_input_file);
    fclose(channels_input_file);
    exit(-1);
  }

  channels_per_node = malloc(sizeof(int)*(tot_nodes));
  for(i = 0; i < tot_nodes; i++){
    channels_per_node[i] = 0;
  }

  fgets(row, 256, channels_input_file);
  while(fgets(row, 256, channels_input_file)!=NULL) {
    sscanf(row, "%ld,%ld,%ld,%ld,%ld,%*d,%*d", &(channel.id), &(channel.edge1), &(channel.edge2), &(channel.node1), &(channel.node2));
    generate_random_channel(channel, net_params.capacity_per_channel, network, random_generator);
    channels_per_node[channel.node1] += 1;
    channels_per_node[channel.node2] += 1;
    ++channel_id_counter;
    edge_id_counter+=2;
  }
  tot_channels = channel_id_counter;
  if(tot_channels == 0){
    fprintf(stderr, "ERROR: it is not possible to generate a network with 0 channels\n");
    fclose(nodes_input_file);
    fclose(channels_input_file);
    exit(-1);
  }

  probability_per_node = malloc(sizeof(double)*tot_nodes);
  for(i=0; i<tot_nodes; i++){
    probability_per_node[i] = ((double)channels_per_node[i])/tot_channels;
  }

  /* scale-free algorithm that creates a network starting from an existing network;
     the probability of connecting nodes is directly proprotional to the number of channels that a node has already open */ 
  for(i=0; i<net_params.n_nodes; i++){
    node_type = LND;
    node = new_node(node_id_counter, node_type,lease_fee_basis, lease_fee_base_sat,channel_fee_max_proportional_thousandths,channel_fee_max_base_msat,funding_weight,funding_feerate_perkw,lease_threshold,
      availabale_funds, funder_policy);
    network->nodes = array_insert(network->nodes, node);
    for(j=0; j<net_params.n_channels; j++){
      connection_probability = gsl_ran_discrete_preproc(node_id_counter, probability_per_node);
      node_to_connect_id = gsl_ran_discrete(random_generator, connection_probability);
      channel.id = channel_id_counter;
      channel.edge1 = edge_id_counter;
      channel.edge2 = edge_id_counter + 1;
      channel.node1 = node->id;
      channel.node2 = node_to_connect_id;
      generate_random_channel(channel, net_params.capacity_per_channel, network, random_generator);
      channel_id_counter++;
      edge_id_counter += 2;
      update_probability_per_node(probability_per_node, channels_per_node, tot_nodes, node->id, node_to_connect_id, channel_id_counter);
    }
    ++node_id_counter;
  }

  fclose(nodes_input_file);
  fclose(channels_input_file);
  free(channels_per_node);
  free(probability_per_node);

  write_network_files(network);

  return network;
}


/* generate a payment-channel network from input files */
struct network* generate_network_from_files(char nodes_filename[256], char channels_filename[256], char edges_filename[256]) {
  char row[2048], node_type_name[256];
  enum node_type node_type;
  struct node* node;
  long id, direction1, direction2, node_id1, node_id2, channel_id, other_direction;
  struct policy policy;
  uint64_t capacity, balance;
  struct channel* channel;
  struct edge* edge;
  struct network* network;
  FILE *nodes_file, *channels_file, *edges_file;
  struct lease_rates rates;
  uint16_t funding_weight; 
  uint16_t lease_fee_basis; 
  uint16_t channel_fee_max_proportional_thousandths; 
	uint32_t lease_fee_base_sat; 
  uint32_t channel_fee_max_base_msat;
  uint32_t funding_feerate_perkw;
  double lease_threshold;
  uint64_t available_funds;
  uint64_t min_their_funding;
  uint64_t max_their_funding;
  uint64_t per_channel_min;
  uint64_t per_channel_max;
  struct funder_policy* funder_policy;

  nodes_file = fopen(nodes_filename, "r");
  if(nodes_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", nodes_filename);
    exit(-1);
  }
  channels_file = fopen(channels_filename, "r");
  if(channels_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", channels_filename);
    fclose(nodes_file);
    exit(-1);
  }
  edges_file = fopen(edges_filename, "r");
  if(edges_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", edges_filename);
    fclose(nodes_file);
    fclose(channels_file);
    exit(-1);
  }

  network = (struct network*) malloc(sizeof(struct network));
  network->nodes = array_initialize(1000);
  network->channels = array_initialize(1000);
  network->edges = array_initialize(2000);

  fgets(row, 2048, nodes_file);
  while(fgets(row, 2048, nodes_file)!=NULL) {
    sscanf(row, "%ld,%[^,],%hd,%d,%hd,%d,%hd,%d,%lf,%ld,%ld,%ld,%ld,%ld", &id, node_type_name,&lease_fee_basis, &lease_fee_base_sat,&channel_fee_max_proportional_thousandths,
    &channel_fee_max_base_msat,&funding_weight,&funding_feerate_perkw,&lease_threshold, &available_funds, &min_their_funding, &max_their_funding, &per_channel_min, &per_channel_max);
    /*printf("%ld,%s,%hd,%d,%hd,%d,%hd,%d,%lf,%ld,%ld,%ld,%ld,%ld\n", id, node_type_name,lease_fee_basis, lease_fee_base_sat,channel_fee_max_proportional_thousandths,
    channel_fee_max_base_msat,funding_weight,funding_feerate_perkw,lease_threshold,available_funds, min_their_funding, max_their_funding, per_channel_min, per_channel_max);*/
    node_type = compare_node_type(node_type_name);
    funder_policy = new_funder_policy(amount_sat(min_their_funding), amount_sat(max_their_funding), amount_sat(per_channel_min), amount_sat(per_channel_max));
    node = new_node(id, node_type, lease_fee_basis, lease_fee_base_sat,channel_fee_max_proportional_thousandths,channel_fee_max_base_msat,funding_weight,funding_feerate_perkw,lease_threshold,
      available_funds, funder_policy);
    
    network->nodes = array_insert(network->nodes, node);
  }
  fclose(nodes_file);

  fgets(row, 2048, channels_file);
  while(fgets(row, 2048, channels_file)!=NULL) {
    sscanf(row, "%ld,%ld,%ld,%ld,%ld,%ld", &id, &direction1, &direction2, &node_id1, &node_id2, &capacity);
    channel = new_channel(id, direction1, direction2, node_id1, node_id2, capacity);
    network->channels = array_insert(network->channels, channel);
  }
  fclose(channels_file);

  int count = 0;

  fgets(row, 2048, edges_file);
  while(fgets(row, 2048, edges_file)!=NULL) {
    count++;
    sscanf(row, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d", &id, &channel_id, &other_direction, &node_id1, &node_id2, &balance, &policy.fee_base, &policy.fee_proportional, &policy.min_htlc, &policy.timelock);
    edge = new_edge(id, channel_id, other_direction, node_id1, node_id2, balance, policy);
    network->edges = array_insert(network->edges, edge);
    node = array_get(network->nodes, node_id1);

    /*printf("ID: %ld, Channel ID: %ld, Other Direction: %ld, Node ID 1: %ld, Node ID 2: %ld, "
               "Balance: %ld, Fee Base: %ld, Fee Proportional: %ld, Min HTLC: %ld, Timelock: %d\n",
               id, channel_id, other_direction, node_id1, node_id2, balance,
               policy.fee_base, policy.fee_proportional, policy.min_htlc, policy.timelock);
    */

    node->open_edges = array_insert(node->open_edges, &(edge->id));
  }
  fclose(edges_file);

  return network;
}


struct network* initialize_network(struct network_params net_params, gsl_rng* random_generator) {
  struct network* network;
  double faulty_prob[2];
  long n_nodes;
  long i, j;
  struct node* node;

  if(net_params.network_from_file)
    network = generate_network_from_files(net_params.nodes_filename, net_params.channels_filename, net_params.edges_filename);
  else
    network = generate_random_network(net_params, random_generator);


  faulty_prob[0] = 1-net_params.faulty_node_prob;
  faulty_prob[1] = net_params.faulty_node_prob;
  network->faulty_node_prob = gsl_ran_discrete_preproc(2, faulty_prob);
  network->block_height = 1;

  n_nodes = array_len(network->nodes);
  for(i=0; i<n_nodes; i++){
    node = array_get(network->nodes, i);
    node->results = (struct element**) malloc(n_nodes*sizeof(struct element*));
    for(j=0; j<n_nodes; j++)
      node->results[j] = NULL;
  }

  return  network;
}

//add function
bool check_node(long from_node_id, long to_node_id, struct network* network){
  int i;
  struct edge* edge;
  struct node* from_node;
  struct node* to_node;

  if(from_node_id == to_node_id){
    return true;
  }

  from_node = array_get(network->nodes, from_node_id);
  to_node = array_get(network->nodes, to_node_id);
  
  if(to_node->type != 1){
    //printf("LNDである");
    return true;
  }

  if(!(to_node->lease_provider)){
    //printf("leaseできない");
    return true;
  }

  return false;
}

bool is_lease_possible(long node_id, struct network* network){
  int i;
  struct node* node;

  for(i=0; i<array_len(network->nodes); i++){
    node = array_get(network->nodes, i);
    if(node_id == node->id ){
      continue;
    }

    if(node->lease_provider){
      return true;
    }
  }
  return false;
}

/*requidity function*/
//チャネルのバランスの割合を調べる
double get_balance_rate(struct edge* edge, struct network* network){
  struct edge* counter_edge;
  double balance;
  double counter_balance;
  double balance_rate;

  balance = (double) edge->balance;
  counter_edge = array_get(network->edges, edge->counter_edge_id);
  counter_balance = (double)counter_edge->balance;
  balance_rate = balance / (balance + counter_balance);

  return balance_rate;
}

uint64_t get_request_amt(struct edge* edge, struct network* network){
  struct channel* channel;

  channel = array_get(network->channels, edge->channel_id);
  return channel->capacity;
}

struct amount_sat get_our_funding(struct amount_sat request_sats){
  struct amount_sat our_funding;

  our_funding.satoshis = request_sats.satoshis * 0.2;

  if(request_sats.satoshis < 546){
    our_funding.satoshis = 546;
    return our_funding;
  }

  return our_funding;
}

bool check_liquidity_node(struct node* node, struct network* network){
  struct edge* edge;
  int i;
  double balance_rate;

  for(i=0; i<array_len(node->open_edges); i++){
    edge = array_get(node->open_edges, i);
    balance_rate = get_balance_rate(edge, network);

    if(balance_rate > node->lease_threshold){
      return false;
    }
  }
  return true;
}

void check_liquidity_network(struct network* network){
  struct node* node;
  struct edge* edge;
  struct edge* counter_edge;
  double balance;
  double counter_balance;
  double balance_rate;
  bool node_requidity;

  for(int i = 0; i < array_len(network->nodes); i++){
      node = array_get(network->nodes, i);
      if(node->type == 1 && check_liquidity_node(node, network)){
        node->lease_provider = true;
      }else{
        node->lease_provider = false;
      }
  }
}

bool check_liquidity(struct edge* edge, struct network* network, double threshold){
  double balance_rate;
  struct amount_sat requested_sats;

  balance_rate = get_balance_rate(edge, network);

  if(balance_rate > threshold){
    return true;
  }

  return false;
}

bool check_lease_effect(long from_node_id, long to_node_id, struct network* network){
  int i;
  struct edge* edge;
  struct node* node;

  node = array_get(network->nodes, from_node_id);

  //ノードとノードの間には一つのチャネルしか繋げないようにする
  //not use
  /*if(array_len(node->open_edges) == array_len(network->nodes) - 1){
    return false;
  }*/

  for(i = 0; i < array_len(node->open_edges); i++){
    edge = array_get(node->open_edges, i);
    if(to_node_id == edge->to_node_id && !check_liquidity(edge, network, node->lease_threshold) ){
      return false;
    }
  }
  return true;
}

struct array* check_lease_provider(struct network* network, long from_node_id, struct amount_sat request_sat, struct amount_sat their_funding){
  struct node* node;
  struct node* from_node;
  struct array* lease_providers;
  struct amount_sat our_funding;
  int i;

  lease_providers = array_initialize(10);

  for(i = 0; i < array_len(network->nodes); i++){
    node = array_get(network->nodes, i);

    if(node->id == from_node_id){
      continue;
    }

    if(node->type != 1){
      continue;
    }

    if(!calculate_our_funding(node->funder_policy, their_funding, amount_sat(node->available_funds), request_sat, &our_funding)){
      continue;
    }
    
    if(check_lease_effect(from_node_id, node->id, network) ){
      lease_providers = array_insert(lease_providers, node);
    }
  }

  return lease_providers;
}

bool accecpt_lease(struct network* network, long node_id, struct amount_sat our_funding, struct amount_sat requested_sat, struct amount_sat their_funding, struct array* lease_providers){
  struct node* node;
  struct node* lease_provider;


  node = array_get(network->nodes, node_id);

  if(!calculate_our_funding(node->funder_policy, our_funding, amount_sat(node->available_funds), requested_sat, &their_funding)){
    for(int i = 0; i < array_len(lease_providers); i++){
      lease_provider = array_get(lease_providers, i);
      if(lease_provider->id == node_id){
        delete_element(lease_providers, i);
      }
    }

    return false;
  }

  return true;
}

long select_lease_provider(struct array* lease_providers){
  struct node* node;
  struct node* lease_provider;
  struct lease_rates* rates;
  int i;
  long lease_provider_id;
  uint64_t min_fee;
  uint64_t fee;

  if(array_len(lease_providers) == 0){
    printf("リースを提供しているノードが存在しません\n");
    return -1;
  }


  node = array_get(lease_providers, 0);
  rates = node->rates;
  min_fee = rates->funding_weight + rates->lease_fee_base_sat + rates->lease_fee_basis;
  lease_provider = node;

  for(i = 1; i < array_len(lease_providers); i++){
    node = array_get(lease_providers, i);
    rates = node->rates;
    fee = rates->funding_weight + rates->lease_fee_base_sat + rates->lease_fee_basis;

    if(min_fee == fee){
      lease_provider = array_len(node->open_edges) > array_len(lease_provider->open_edges) ? node : lease_provider;
    }

    if(min_fee > fee){
      min_fee = fee;
      lease_provider = node;
    }
  }

  return lease_provider->id;
}
//end

/* open a new channel during the simulation */
/* currenlty NOT USED */
void open_channel(struct network* network, gsl_rng* random_generator){
  struct channel channel;
  channel.id = array_len(network->channels);
  channel.edge1 = array_len(network->edges);
  channel.edge2 = array_len(network->edges) + 1;
  channel.node1 = gsl_rng_uniform_int(random_generator, array_len(network->nodes));
  do{
    channel.node2 = gsl_rng_uniform_int(random_generator, array_len(network->nodes));
  } while(channel.node2==channel.node1);
  generate_random_channel(channel, 1000, network, random_generator);
}

void open_lease_channel(struct network* network, struct node* from_node, struct edge* edge, gsl_rng* random_generator){
  struct amount_sat requested_sat;
  struct amount_sat our_funding;
  struct amount_sat their_funding;
  struct channel channel;
  struct array* lease_providers;
  channel.id = array_len(network->channels);
  channel.edge1 = array_len(network->edges);
  channel.edge2 = array_len(network->edges) + 1;
  channel.node1 = from_node->id;

  from_node->lease_provider = false;
  requested_sat.satoshis =  get_request_amt(edge, network);
  printf("edge: %ld\n", edge->id);
  printf("request_amt: %llu\n", requested_sat.satoshis);
  our_funding = get_our_funding(requested_sat);

  printf("リースリクエスト\n");

  if( !(is_lease_possible(channel.node1, network)) ){
    //printf("leaseできるノードがありません");
    return;
  }

  if(check_available_funds(from_node, our_funding) == false){
    printf("our_funding: %lld\n", our_funding.satoshis);
    printf("available_funds: %ld\n", from_node->available_funds);
    printf("所持金が足りません");
    return;
  }

  lease_providers = check_lease_provider(network, channel.node1, requested_sat, our_funding);

  do{
    printf("%ld\n", array_len(lease_providers));
    channel.node2 = select_lease_provider(lease_providers);
  } while(accecpt_lease(network, channel.node2, our_funding, requested_sat, their_funding, lease_providers) == false);
  
  printf("リースノード選択成功: ノードID %ld", channel.node2);

  if(channel.node2 == -1){
    return;
  }

  //do{
  //  channel.node2 = gsl_rng_uniform_int(random_generator, array_len(network->nodes));
  //} while(check_node(channel.node1, channel.node2, network));
  lease_channel(channel, our_funding, requested_sat, network, random_generator);
}
