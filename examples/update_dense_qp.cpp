#include <qp/dense/dense.hpp> // load the dense solver backend
#include <test/include/util.hpp> // used for generating a random convex Qp

using namespace qp;
using T = double;
int main()
{
	dense::isize dim = 10;
	dense::isize n_eq(dim / 4);
	dense::isize n_in(dim / 4);
    // generate a random qp
    T sparsity_factor(0.15);
	T strong_convexity_factor(1.e-2);
	Qp<T> qp{
			random_with_dim_and_neq_and_n_in,
			dim,
			n_eq,
			n_in,
			sparsity_factor,
			strong_convexity_factor};

    dense::QP<T> Qp(dim, n_eq, n_in); // create the QP object
	Qp.init(qp.H, qp.g, qp.A, qp.b, qp.C, qp.u, qp.l); // initialize the model
    Qp.solve(); // solve the problem
    // a new Qp problem
	Qp<T> qp2{
			random_with_dim_and_neq_and_n_in,
			dim,
			n_eq,
			n_in,
			sparsity_factor,
			strong_convexity_factor};
    // re update the model
    Qp.update(qp2.H, qp2.g, qp2.A, qp2.b, qp2.C, qp2.u, qp2.l);
    // solve it
    Qp.solve();
}
