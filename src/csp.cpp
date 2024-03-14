#include "csp.h"

/* ============================ INTERNAL IMPLEMENTATION ============================ */

static
b8 csp_constraint_satisfied(Constraint *constraint) {
	b8 result;

	switch(constraint->type) {
	default:
		result = true;
		break;
	}

	return result;
}

Variable *csp_least_remaining_values_heuristic(CSP *csp) {
	return csp->variables.first();
}

s64 csp_least_constraining_value_heuristic(CSP *csp, Variable *variable) {
	return variable->domain->values[0];
}


/* ============================ API ============================ */

Domain *create_csp_domain(CSP *csp, s64 values[], s64 value_count) {
	Domain *domain = csp->domains.push();
	domain->count  = value_count;
	domain->values = (s64 *) csp->allocator->allocate(domain->count * sizeof(s64));
	memcpy(domain->values, values, value_count * sizeof(s64));
	return domain;
}

Variable *create_csp_variable(CSP *csp, Domain *domain, string display_name) {
	Variable *variable        = csp->variables.push();
	variable->domain          = domain;
	variable->display_name    = display_name;
	variable->assigned_value  = 0;
	variable->value_is_banned = (b8 *) csp->allocator->allocate(variable->domain->count * sizeof(b8));
	memset(variable->value_is_banned, 0, variable->domain->count * sizeof(b8));
	return variable;
}

void create_csp_binary_constraint(CSP *csp, Constraint_Type type, Variable *lhs, Variable *rhs, s64 value) {
	Constraint *constraint = csp->contraints.push();
	constraint->type       = type;
	constraint->lhs        = lhs;
	constraint->rhs        = rhs;
	constraint->value      = value;
}


b8 solve_csp(CSP *csp) {
	return false;
}