#ifndef LEASE_RATES_H
#define LEASE_RATES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include "network.h"

/* OP_0 + PUSH(20-byte-hash) */
#define BITCOIN_SCRIPTPUBKEY_P2WPKH_LEN (1 + 1 + 20)
#define MSAT_PER_SAT ((uint64_t)1000)


struct lease_rates {
    uint16_t funding_weight; 
    uint16_t lease_fee_basis; 
    uint16_t channel_fee_max_proportional_thousandths; 
	uint32_t lease_fee_base_sat; 
    uint32_t channel_fee_max_base_msat; 
};

struct amount_sat{
    unsigned long long satoshis;
};

struct amount_msat {
	/* Amount in millisatoshis. */
	uint64_t millisatoshis;
};

struct funder_policy {
    struct amount_sat min_their_funding;
    struct amount_sat max_their_funding;
    struct amount_sat per_channel_min;
    struct amount_sat per_channel_max;
};

static inline bool add_overflows_u64(uint64_t a, uint64_t b);

static inline bool mul_overflows_u64(uint64_t a, uint64_t b);

struct amount_sat amount_sat(uint64_t satoshis);

bool amount_sat_zero(struct amount_sat a);

bool amount_sat_less(struct amount_sat a, struct amount_sat b);

bool amount_sat_scale(struct amount_sat *val, struct amount_sat sat, double scale);

struct amount_sat amount_sat_div(struct amount_sat sat, uint64_t div);

bool amount_sat_add(struct amount_sat *val,struct amount_sat a,struct amount_sat b);

struct amount_sat amount_tx_fee(uint32_t fee_per_kw, size_t weight);

bool lease_rates_calc_fee(const struct lease_rates *rates,struct amount_sat accept_funding_sats, struct amount_sat requested_sats, uint32_t onchain_feerate, struct amount_sat *fee);

struct lease_rates * default_lease_rates(void);

struct lease_rates* lease_rates_initialize(uint16_t lease_fee_basis,uint32_t lease_fee_base_sat,uint16_t channel_fee_max_proportional_thousandths,
uint32_t channel_fee_max_base_msat,uint16_t funding_weight);

bool check_available_funds(struct node* node, struct amount_sat request_sat);

struct funder_policy* new_funder_policy(struct amount_sat min_their_funding,struct amount_sat max_their_funding,struct amount_sat per_channel_min,struct amount_sat per_channel_max);

const bool calculate_our_funding(struct funder_policy *policy,struct amount_sat their_funding,struct amount_sat available_funds,struct amount_sat requested_lease,struct amount_sat *our_funding);

#endif