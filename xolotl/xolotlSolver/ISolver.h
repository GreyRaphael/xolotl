#ifndef ISOLVER_H
#define ISOLVER_H

// Includes
#include <map>
#include <ISolverHandler.h>
#include <Options.h>

using namespace xolotlCore;

namespace xolotlSolver {

/**
 * Realizations of this interface are responsible for solving the set of
 * advection-diffusion-reaction equations that govern the evolution of
 * clusters in plasma surface interactions.
 */
class ISolver {

public:

	/**
	 * The destructor
	 */
	virtual ~ISolver(){}

	/**
	 * This operation transfers the input arguments passed to the program on
	 * startup to the solver. These options are static options specified at
	 * the start of the program whereas the options passed to setOptions() may
	 * change.
	 *
	 * Const correctness is violated here because char ** is not const correct
	 * in main().
	 *
	 * @param argc The number of command line arguments
	 * @param argv The array of command line arguments
	 */
	virtual void setCommandLineOptions(int argc, char **argv) = 0;

	/**
	 * This operation sets up the mesh that will be used by the solver and
	 * initializes the data on that mesh. This operation will throw an exception
	 * of type std::string if the mesh can not be setup.
	 */
	virtual void setupMesh() = 0;

	/**
	 * This operation performs all necessary initialization for the solver
	 * possibly including but not limited to setting up MPI and loading initial
	 * conditions. If the solver can not be initialized, this operation will
	 * throw an exception of type std::string.
	 */
	virtual void initialize() = 0;

	/**
	 * This operation directs the Solver to perform the solve. If the solve
	 * fails, it will throw an exception of type std::string.
	 */
	virtual void solve(Options &options) = 0;

	/**
	 * This operation performs all necessary finalization for the solver
	 * including but not limited to cleaning up memory, finalizing MPI and
	 * printing diagnostic information. If the solver can not be finalized,
	 * this operation will throw an exception of type std::string.
	 */
	virtual void finalize() = 0;

}; //end class ISolver

} /* namespace xolotlSolver */
#endif
