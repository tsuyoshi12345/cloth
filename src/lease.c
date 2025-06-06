#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "../include/lease.h"
#include "../include/network.h"

//overflows.h参照
static inline bool add_overflows_u64(uint64_t a, uint64_t b)
{
	return (a + b) < a;
}

static inline bool mul_overflows_u64(uint64_t a, uint64_t b)
{
	uint64_t ret;

	if (a == 0)
		return false;
	ret = a * b;
	return (ret / a != b);
}

//amount.c参照
struct amount_sat amount_sat(uint64_t satoshis)
{
	struct amount_sat sat;

	sat.satoshis = satoshis;
	return sat;
}

bool amount_sat_zero(struct amount_sat a)
{
	return a.satoshis == 0;
}

bool amount_sat_to_msat(struct amount_msat *msat,
			struct amount_sat sat)
{
	if (mul_overflows_u64(sat.satoshis, MSAT_PER_SAT))
		return false;
	msat->millisatoshis = sat.satoshis * MSAT_PER_SAT;
	return true;
}

bool amount_msat_to_sat(struct amount_sat *sat,
			struct amount_msat msat)
{
	if (msat.millisatoshis % MSAT_PER_SAT)
		return false;
	sat->satoshis = msat.millisatoshis / MSAT_PER_SAT;
	return true;
}

bool amount_sat_less(struct amount_sat a, struct amount_sat b){
	return a.satoshis < b.satoshis;
}

bool amount_sat_greater(struct amount_sat a, struct amount_sat b)
{
	return a.satoshis > b.satoshis;
}

bool amount_sat_scale(struct amount_sat *val, struct amount_sat sat, double scale){
	double scaled = sat.satoshis * scale;
	/* If mantissa is < 64 bits, a naive "if (scaled >
	 * UINT64_MAX)" doesn't work.  Stick to powers of 2. */
	if (scaled >= (double)((uint64_t)1 << 63) * 2)
		return false;
	val->satoshis = scaled;
	return true;
}

struct amount_sat amount_sat_div(struct amount_sat sat, uint64_t div)
{
	sat.satoshis /= div;
	return sat;
}



bool amount_sat_add(struct amount_sat *val,
				       struct amount_sat a,
				       struct amount_sat b)
{
	if (add_overflows_u64(a.satoshis, b.satoshis))
		return false;

	val->satoshis = a.satoshis + b.satoshis;
	return true;
}

bool amount_sat_sub(struct amount_sat *val,
				       struct amount_sat a,
				       struct amount_sat b)
{
	if (a.satoshis < b.satoshis)
		return false;

	val->satoshis = a.satoshis - b.satoshis;
	return true;
}

struct amount_sat amount_tx_fee(uint32_t fee_per_kw, size_t weight)
{
	struct amount_sat fee;

	/* If this overflows, weight must be > 2^32, which is not a real tx */
	assert(!mul_overflows_u64(fee_per_kw, weight));
	fee.satoshis = (uint64_t)fee_per_kw * weight / 1000;

	return fee;
}

bool lease_rates_calc_fee(const struct lease_rates *rates,
			  struct amount_sat accept_funding_sats,
			  struct amount_sat requested_sats,
			  uint32_t onchain_feerate,
			  struct amount_sat *fee)
{
	struct amount_sat lease_fee, basis_sat, tx_fee;
	/* BOLT- #2:
	 * The lease fee is calculated as:
	 * `lease_fee_base_sat` +
	 * min(`accept_channel2`.`funding_satoshis`, `open_channel2`.`requested_sats`) * `lease_fee_basis` / 10_000 +
	 * `funding_weight` * `funding_feerate_perkw` / 1000
	 */
	printf("Funding Weight: %u\n", rates->funding_weight);
    printf("Lease Fee Basis: %u\n", rates->lease_fee_basis);
    printf("Channel Fee Max Proportional Thousandths: %u\n", rates->channel_fee_max_proportional_thousandths);
    printf("Lease Fee Base Sat: %u\n", rates->lease_fee_base_sat);
    printf("Channel Fee Max Base Msat: %u\n", rates->channel_fee_max_base_msat);
	lease_fee = amount_sat(rates->lease_fee_base_sat);
	basis_sat = amount_sat_less(accept_funding_sats, requested_sats) ? accept_funding_sats : requested_sats;

	if (!amount_sat_scale(&basis_sat, basis_sat,rates->lease_fee_basis))
		return false;

	basis_sat = amount_sat_div(basis_sat, 1000);

	if (!amount_sat_add(&lease_fee, lease_fee, basis_sat))
		return false;

	tx_fee = amount_tx_fee(onchain_feerate, rates->funding_weight);

	if (!amount_sat_add(&lease_fee, lease_fee, tx_fee))
		return false;

	*fee = lease_fee;

	return true;
}

struct lease_rates * default_lease_rates(void)
{ 
	struct lease_rates *rates = malloc(sizeof(struct lease_rates));

	/* Default basis is .65%, (7.8% APR) */
	rates->lease_fee_basis = 65;
	/* 2000sat base rate */
	rates->lease_fee_base_sat = 2000;
	/* Max of 100,000ppm (10%) */
	rates->channel_fee_max_proportional_thousandths = 100;
	/* Max of 5000sat */
	rates->channel_fee_max_base_msat = 5000000;

	/* Let's set our default max weight to two inputs + an output
	 * (use helpers b/c elements) */
	 /*このパラメータの詳細はわかっていない*/
	rates->funding_weight = 666;

	return rates;
}

struct lease_rates* lease_rates_initialize(uint16_t lease_fee_basis,uint32_t lease_fee_base_sat,uint16_t channel_fee_max_proportional_thousandths,
uint32_t channel_fee_max_base_msat,uint16_t funding_weight){
	struct lease_rates *rates = malloc(sizeof(struct lease_rates));

	/* Default basis is .65%, (7.8% APR) */
	rates->lease_fee_basis = lease_fee_basis;
	/* 2000sat base rate */
	rates->lease_fee_base_sat = lease_fee_base_sat;
	/* Max of 100,000ppm (10%) */
	rates->channel_fee_max_proportional_thousandths = channel_fee_max_proportional_thousandths;
	/* Max of 5000sat */
	rates->channel_fee_max_base_msat = channel_fee_max_base_msat;

	/* Let's set our default max weight to two inputs + an output
	 * (use helpers b/c elements) */
	 /*このパラメータの詳細はわかっていない*/
	rates->funding_weight = funding_weight;

	return rates;
	
}

bool check_available_funds(struct node* node, struct amount_sat require_sat){
	//利用可能な金額より高いかどうか確認する
	return node->available_funds > require_sat.satoshis;
};

struct funder_policy* new_funder_policy(struct amount_sat min_their_funding,struct amount_sat max_their_funding,struct amount_sat per_channel_min,struct amount_sat per_channel_max){
	struct funder_policy* policy;
  	policy = malloc(sizeof(struct funder_policy));

	policy->min_their_funding = min_their_funding;
	policy->max_their_funding = max_their_funding;
	policy->per_channel_min = per_channel_min;
	policy->per_channel_max = per_channel_max;

	return policy;
}

const bool calculate_our_funding(struct funder_policy *policy,
		      struct amount_sat their_funding, //openerの資金
		      struct amount_sat available_funds, //leaserの利用可能資金
		      struct amount_sat requested_lease, //リクエストされた資金
		      struct amount_sat *our_funding) //　leaserの資金
{

	/* Are we only funding lease requests ? */
	if (amount_sat_zero(requested_lease)) {
		*our_funding = amount_sat(0);
		//printf("リクエストされた金額が０です\n");
		return false;
	}

	/* Are they funding enough ? */
	if (amount_sat_less(their_funding, policy->min_their_funding)) {
		//printf("相手のノードが提供する金額が少ないです\n");
		*our_funding = amount_sat(0);
		return false;
	}


	/* Are they funding too much ? */
	if (amount_sat_greater(their_funding, policy->max_their_funding)) {
		//printf("相手のノードが提供する金額が多すぎます\n");
		*our_funding = amount_sat(0);
		
	}

	/* What's our amount, given our policy */
	*our_funding = requested_lease;

	/* Is our_funding more than we want to fund in a channel?
	 * if so set at our desired per-channel max */
	if (amount_sat_greater(*our_funding, policy->per_channel_max))
		*our_funding = policy->per_channel_max;

	/* Is our_funding more than we have available? if so
	 * set to max available */
	if (amount_sat_greater(*our_funding, available_funds))
		*our_funding = available_funds;

	/* Are we putting in less than last time + it's a lease?
	 * Return an error as a convenience to the buyer */
	if (amount_sat_less(*our_funding, requested_lease)) {
		//printf("リクエストされた金額を提供できません\n");
		*our_funding = amount_sat(0);
		//printf("our_funding: %lld\n", our_funding->satoshis);
		//printf("request_lease: %lld\n", requested_lease);
		return false;
	}

	/* Is our_funding less than our per-channel minimum?
	 * if so, don't fund */
	if (amount_sat_less(*our_funding, policy->per_channel_min)) {
		//printf("提供する金額が少ないです\n");
		*our_funding = amount_sat(0);
		return false;
	}

	//結果を出力する
	//printf("our_funding: %lld\n", our_funding->satoshis);
	//printf("request_lease: %lld\n", requested_lease.satoshis);

	return true;
}


