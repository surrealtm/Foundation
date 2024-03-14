#pragma once

#include "foundation.h"
#include "string.h"
#include "memutils.h"

/* This file provides functionality for solving a logical constraint satisfaction problem.
 * Each problem contains a number of constraints which need to be solved, so that the
 * entire problem is solved. Each constraint can be unary or binary, and each constraint
 * operates on one or two variables. Each variable has a discrete space of possible values.
 * This space is called domain. This solver only supports integers as values in a domain,
 * for more complex value types these integers may be used as an index into the user-level
 * domain.
 * A solution maps one value to each variable, so that this mapping fulfills all given
 * constraints.
 */

enum Constraint_Type {
	CONSTRAINT_equality,
	CONSTRAINT_inequality,
};

struct Domain {
	s64 count;
	s64 *values;
};

struct Variable {
	// --- User input
	Domain *domain;
	string display_name;
	
	// --- User output
	s64 assigned_value;

	// --- Internal state
	b8 *value_is_banned; // Maps a boolean to each value in this variables domain, as to whether it has been banned through some inference algorithm.
};

struct Constraint {
	Constraint_Type type;
	Variable *lhs;
	Variable *rhs;
	s64 value; // This value may be used for different constraints, i.e. as an offset for the equality constraint (   lhs == rhs + value)
};

struct CSP { // Constraint Satisfaction Problem
	Allocator *allocator = Default_Allocator;
	Linked_List<Variable> variables;
	Linked_List<Constraint> contraints;
	Linked_List<Domain> domains;
};

Domain *create_csp_domain(CSP *csp, s64 values[], s64 value_count);
Variable *create_csp_variable(CSP *csp, Domain *domain, string display_name);
void create_csp_binary_constraint(CSP *csp, Constraint_Type type, Variable *lhs, Variable *rhs, s64 value = 0);

b8 solve_csp(CSP *csp);