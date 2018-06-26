#ifndef INTERFACE_H
#define INTERFACE_H
#include <PetscSolver.h>

/**
 * Class defining the method to be coupled to another code through MOOSEApps
 */
class XolotlInterface {

public:

	/**
	 * The default constructor
	 */
	XolotlInterface() {}

	/**
	 * The destructor
	 */
	~XolotlInterface() {}

	/**
	 * Initialize all the options and handlers
	 */
	std::shared_ptr<xolotlSolver::PetscSolver> initializeXolotl();

	/**
	 * Run the PETSc solve
	 */
	void solveXolotl(std::shared_ptr<xolotlSolver::PetscSolver> solver);

}; // End class interface
#endif