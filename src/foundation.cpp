#include "foundation.h"
#include "os_specific.h"
#include "memutils.h"
#include "string.h"
#include "csp.h"

int main() {
	s64 values[] = { 1, 2, 3};

	CSP csp;
	Domain *domain = create_csp_domain(&csp, values, 3);
	Variable *v1 = create_csp_variable(&csp, domain, "v1"_s);
	Variable *v2 = create_csp_variable(&csp, domain, "v2"_s);
	Variable *v3 = create_csp_variable(&csp, domain, "v3"_s);
	Variable *v4 = create_csp_variable(&csp, domain, "v4"_s);
	Variable *v5 = create_csp_variable(&csp, domain, "v5"_s);

	create_csp_binary_constraint(&csp, CONSTRAINT_inequality, v1, v3);
	create_csp_binary_constraint(&csp, CONSTRAINT_inequality, v1, v5);
	create_csp_binary_constraint(&csp, CONSTRAINT_equality, v2, v1, 1);
	create_csp_binary_constraint(&csp, CONSTRAINT_inequality, v2, v3);
	create_csp_binary_constraint(&csp, CONSTRAINT_inequality, v3, v4);
	create_csp_binary_constraint(&csp, CONSTRAINT_inequality, v3, v5);
	create_csp_binary_constraint(&csp, CONSTRAINT_inequality, v4, v5);
	
	solve_csp(&csp);

	for(Variable & all : csp.variables) {
		printf("'%.*s': %lld\n", all.display_name.count, all.display_name.data, all.assigned_value);
	}

	return 0;
}