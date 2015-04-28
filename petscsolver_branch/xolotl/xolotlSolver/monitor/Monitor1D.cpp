// Includes
#include "PetscSolver.h"
#include <xolotlPerf.h>
#include <VizHandlerRegistryFactory.h>
#include <PlotType.h>
#include <CvsXDataProvider.h>
#include <CvsXYDataProvider.h>
#include <LabelProvider.h>
#include <petscts.h>
#include <petscsys.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <HDF5Utils.h>

namespace xolotlSolver {

// Declaration of the functions defined in Monitor.cpp
extern PetscErrorCode monitorTime(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx);
extern PetscErrorCode computeHeliumFluence(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx);
extern PetscErrorCode monitorPerf(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx);

// Declaration of the variables defined in Monitor.cpp
extern std::shared_ptr<xolotlViz::IPlot> perfPlot;
extern double previousTime;

//! The pointer to the plot used in monitorScatter1D.
std::shared_ptr<xolotlViz::IPlot> scatterPlot1D;
//! The pointer to the series plot used in monitorSeries1D.
std::shared_ptr<xolotlViz::IPlot> seriesPlot1D;
//! The pointer to the 2D plot used in MonitorSurface.
std::shared_ptr<xolotlViz::IPlot> surfacePlot1D;
//! The variable to store the interstitial flux at the previous time step.
double previousIFlux1D = 0.0;
//! How often HDF5 file is written
PetscInt hdf5Stride1D = 0;
//! HDF5 output file name
std::string hdf5OutputName1D = "xolotlStop.h5";
// Declare the vector that will store the Id of the helium clusters
std::vector<int> heIndices1D;
// Declare the vector that will store the weight of the helium clusters
// (their He composition)
std::vector<int> heWeights1D;
// Variable to indicate whether or not the fact that the concentration of the biggest
// cluster in the network is higher than 1.0e-16 should be printed.
// Becomes false once it is printed.
bool printMaxClusterConc1D = true;

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "startStop1D")
/**
 * This is a monitoring method that will save an hdf5 file at each time step.
 * HDF5 is handling the parallel part, so no call to MPI here.
 */
PetscErrorCode startStop1D(TS ts, PetscInt timestep, PetscReal time, Vec solution,
		void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, Mx;

	PetscFunctionBeginUser;

	// Don't do anything if it is not on the stride
	if (timestep % hdf5Stride1D != 0)
		PetscFunctionReturn(0);

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID (important when it is running in parallel)
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);
	// Get the size of the total grid
	ierr = DMDAGetInfo(da, PETSC_IGNORE, &Mx, PETSC_IGNORE, PETSC_IGNORE,
	PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE,
	PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE,
	PETSC_IGNORE);CHKERRQ(ierr);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network
	auto network = solverHandler->getNetwork();

	// Network size
	const int networkSize = network->size();

	// Open the already created HDF5 file
	xolotlCore::HDF5Utils::openFile(hdf5OutputName1D);

	// Get the current time step
	double currentTimeStep;
	ierr = TSGetTimeStep(ts, &currentTimeStep);CHKERRQ(ierr);

	// Add a concentration sub group
	xolotlCore::HDF5Utils::addConcentrationSubGroup(timestep, networkSize, time,
			currentTimeStep);

	// Loop on the full grid
	for (int i = 0; i < Mx; i++) {
		// Size of the concentration that will be stored
		int concSize = -1;
		// Vector for the concentrations
		std::vector<std::vector<double> > concVector;

		// If it is the locally owned part of the grid
		if (i >= xs && i < xs + xm) {
			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[i];

			// Loop on the concentrations
			concVector.clear();
			for (int l = 0; l < networkSize; l++) {
				if (gridPointSolution[l] > 1.0e-16) {
					// Create the concentration vector for this cluster
					std::vector<double> conc;
					conc.push_back((double) l);
					conc.push_back(gridPointSolution[l]);

					// Add it to the main vector
					concVector.push_back(conc);
				}
			}

			// Send the size of the vector to the other processes
			concSize = concVector.size();
			// Loop on all the processes
			for (int l = 0; l < worldSize; l++) {
				// Skip its own
				if (l == procId)
					continue;

				// Send the size
				MPI_Send(&concSize, 1, MPI_INT, l, 0, MPI_COMM_WORLD);
			}
		}

		// Else: only receive the conc size
		else {
			MPI_Recv(&concSize, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);
		}

		// Skip the grid point if the size is 0
		if (concSize == 0)
			continue;

		// All processes must create the dataset
		xolotlCore::HDF5Utils::addConcentrationDataset(concSize, i);

		// If it is the locally owned part of the grid
		if (i >= xs && i < xs + xm) {
			// Fill the dataset
			xolotlCore::HDF5Utils::fillConcentrations(concVector, i);
		}
	}

	// Finalize the HDF5 file
	xolotlCore::HDF5Utils::closeFile();

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "computeHeliumRetention1D")
/**
 * This is a monitoring method that will compute the helium retention
 */
PetscErrorCode computeHeliumRetention1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	int xs, xm;

	PetscFunctionBeginUser;

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the flux handler that will be used to compute fluxes.
	auto fluxHandler = solverHandler->getFluxHandler();

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// Get the array of concentration
	PetscReal **solutionArray;
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Store the concentration over the grid
	double heConcentration = 0;

	// Declare the pointer for the concentrations at a specific grid point
	PetscReal *gridPointSolution;

	// Loop on the grid
	for (int xi = xs; xi < xs + xm; xi++) {
		// Get the pointer to the beginning of the solution data for this grid point
		gridPointSolution = solutionArray[xi];

		// Loop on all the indices
		for (int i = 0; i < heIndices1D.size(); i++) {
			// Add the current concentration times the number of helium in the cluster
			// (from the weight vector)
			heConcentration += gridPointSolution[heIndices1D[i]] * heWeights1D[i]
			                                     * (grid[xi] - grid[xi-1]);
		}
	}

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);
	// Get the current process ID
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Master process
	if (procId == 0) {
		// Loop on all the other processes
		for (int i = 1; i < worldSize; i++) {
			double otherConcentration = 0.0;

			// Receive the value from the other processes
			MPI_Recv(&otherConcentration, 1, MPI_DOUBLE, i, 1, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);

			// Add them to the master one
			heConcentration += otherConcentration;
		}

		// Get the fluence
		double heliumFluence = fluxHandler->getHeFluence();

		// Print the result
		std::cout << "\nTime: " << time << std::endl;
		std::cout << "Helium retention = "
				<< 100.0 * (heConcentration / heliumFluence) << " %"
				<< std::endl;
		std::cout << "Helium concentration = " << heConcentration << std::endl;
		std::cout << "Helium fluence = " << heliumFluence << "\n" << std::endl;

		// Uncomment to write the retention and the fluence in a file
		std::ofstream outputFile;
		outputFile.open("retentionOut.txt", ios::app);
		outputFile << heliumFluence << " "
				<< 100.0 * (heConcentration / heliumFluence) << std::endl;
		outputFile.close();
	}

	else {
		// Send the value of the timer to the master process
		MPI_Send(&heConcentration, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "computeCumulativeHelium1D")
/**
 * This is a monitoring method that will compute the cumulative distribution of helium
 */
PetscErrorCode computeCumulativeHelium1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	int xs, xm;

	PetscFunctionBeginUser;

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID (important when it is running in parallel)
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the flux handler that will be used to compute fluxes.
	auto fluxHandler = solverHandler->getFluxHandler();

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the physical grid and its length
	auto grid = solverHandler->getXGrid();
	int xSize = grid.size();

	// Get the array of concentration
	PetscReal **solutionArray;
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Store the concentration over the grid
	double heConcentration = 0.0;

	// Declare the pointer for the concentrations at a specific grid point
	PetscReal *gridPointSolution;

	// Create the output file
	std::ofstream outputFile;
	if (procId == 0) {
		std::stringstream name;
		name << "heliumCumul_" << timestep << ".dat";
		outputFile.open(name.str());
	}

	// Loop on the entire grid
	for (int xi = 0; xi < xSize; xi++) {
		// Wait for everybody at each grid point
		MPI_Barrier(PETSC_COMM_WORLD);

		// Set x
		double x = grid[xi];

		// Initialize the helium concentration at this grid point
		double heLocalConc = 0.0;

		// Check if this process is in charge of xi
		if (xi >= xs && xi < xs + xm) {
			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[xi];
			// Compute the total helium concentration at this grid point
			// Loop on all the indices
			for (int i = 0; i < heIndices1D.size(); i++) {
				// Add the current concentration times the number of helium in the cluster
				// (from the weight vector)
				heLocalConc += gridPointSolution[heIndices1D[i]] * heWeights1D[i]
						                                     * (grid[xi] - grid[xi-1]);
			}

			// If this is not the master process, send the value
			if (procId != 0) {
				MPI_Send(&heLocalConc, 1, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
			}
		}
		// If this process is not in charge of xi but is the master one, receive the value
		else if (procId == 0) {
			MPI_Recv(&heLocalConc, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 2, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);
		}

		// The master process writes computes the cumulative value and writes in the file
		if (procId == 0) {
			heConcentration += heLocalConc;
			outputFile << x << " " << heConcentration << std::endl;
		}
	}

	// Close the file
	if (procId == 0) {
		outputFile.close();
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "monitorScatter1D")
/**
 * This is a monitoring method that will save 1D plots of one concentration
 */
PetscErrorCode monitorScatter1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, xi;
	double x;

	PetscFunctionBeginUser;

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID (important when it is running in parallel)
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network
	auto network = solverHandler->getNetwork();

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// Choice of the cluster to be plotted
	int iCluster = 0;

	if (procId == 0) {
		// Create a Point vector to store the data to give to the data provider
		// for the visualization
		auto myPoints = std::make_shared<std::vector<xolotlViz::Point> >();

		// Loop on the grid
		for (xi = xs; xi < xs + xm; xi++) {
			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[xi];

			// Create a Point with the concentration[iCluster] as the value
			// and add it to myPoints
			xolotlViz::Point aPoint;
			aPoint.value = gridPointSolution[iCluster];
			aPoint.t = time;
			aPoint.x = grid[xi];
			myPoints->push_back(aPoint);
		}

		// Loop on the other processes
		for (int i = 1; i < worldSize; i++) {
			// Get the size of the local grid of that process
			int localSize = 0;
			MPI_Recv(&localSize, 1, MPI_INT, i, 3, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);

			// Loop on their grid
			for (int k = 0; k < localSize; k++) {
				// Get the position
				MPI_Recv(&x, 1, MPI_DOUBLE, i, 3, MPI_COMM_WORLD,
						MPI_STATUS_IGNORE);

				// and the concentration
				double conc = 0.0;
				MPI_Recv(&conc, 1, MPI_DOUBLE, i, 3, MPI_COMM_WORLD,
						MPI_STATUS_IGNORE);

				// Create a Point with the concentration[iCluster] as the value
				// and add it to myPoints
				xolotlViz::Point aPoint;
				aPoint.value = conc;
				aPoint.t = time;
				aPoint.x = x;
				myPoints->push_back(aPoint);
			}
		}

		// Get the data provider and give it the points
		scatterPlot1D->getDataProvider()->setPoints(myPoints);

		// Get the iCluster cluster to have access to its name
		auto reactants = network->getAll();
		auto cluster = (PSICluster *) reactants->at(iCluster);

		// Change the title of the plot and the name of the data
		std::stringstream title;
		title << cluster->getName();
		scatterPlot1D->getDataProvider()->setDataName(title.str());
		title << " concentration";
		scatterPlot1D->plotLabelProvider->titleLabel = title.str();
		// Give the time to the label provider
		std::stringstream timeLabel;
		timeLabel << "time: " << std::setprecision(4) << time << "s";
		scatterPlot1D->plotLabelProvider->timeLabel = timeLabel.str();
		// Get the current time step
		PetscReal currentTimeStep;
		ierr = TSGetTimeStep(ts, &currentTimeStep);CHKERRQ(ierr);
		// Give the timestep to the label provider
		std::stringstream timeStepLabel;
		timeStepLabel << "dt: " << std::setprecision(4) << currentTimeStep
				<< "s";
		scatterPlot1D->plotLabelProvider->timeStepLabel = timeStepLabel.str();

		// Render and save in file
		std::stringstream fileName;
		fileName << cluster->getName() << "_scatter_TS" << timestep << ".pnm";
		scatterPlot1D->write(fileName.str());
	}

	else {
		// Send the value of the local grid size to the master process
		MPI_Send(&xm, 1, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);

		// Loop on the grid
		for (xi = xs; xi < xs + xm; xi++) {
			// Dump x
			x = grid[xi];

			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[xi];

			// Send the value of the local position to the master process
			MPI_Send(&x, 1, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);

			// Send the value of the concentration to the master process
			MPI_Send(&gridPointSolution[iCluster], 1, MPI_DOUBLE, 0, 3,
					MPI_COMM_WORLD);
		}
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "monitorSeries1D")
/**
 * This is a monitoring method that will save 1D plots of many concentrations
 */
PetscErrorCode monitorSeries1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, xi;
	double x;

	PetscFunctionBeginUser;

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID (important when it is running in parallel)
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network and its size
	auto network = solverHandler->getNetwork();
	const int networkSize = network->size();

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// To plot a maximum of 18 clusters of the whole benchmark
	const int loopSize = std::min(18, networkSize);

	if (procId == 0) {
		// Create a Point vector to store the data to give to the data provider
		// for the visualization
		std::vector<std::vector<xolotlViz::Point> > myPoints(loopSize);

		// Loop on the grid
		for (xi = xs; xi < xs + xm; xi++) {
			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[xi];

			for (int i = 0; i < loopSize; i++) {
				// Create a Point with the concentration[iCluster] as the value
				// and add it to myPoints
				xolotlViz::Point aPoint;
				aPoint.value = gridPointSolution[i];
				aPoint.t = time;
				aPoint.x = grid[xi];
				myPoints[i].push_back(aPoint);
			}
		}

		// Loop on the other processes
		for (int i = 1; i < worldSize; i++) {
			// Get the size of the local grid of that process
			int localSize = 0;
			MPI_Recv(&localSize, 1, MPI_INT, i, 4, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);

			// Loop on their grid
			for (int k = 0; k < localSize; k++) {
				// Get the position
				MPI_Recv(&x, 1, MPI_DOUBLE, i, 4, MPI_COMM_WORLD,
						MPI_STATUS_IGNORE);

				for (int j = 0; j < loopSize; j++) {
					// and the concentrations
					double conc;
					MPI_Recv(&conc, 1, MPI_DOUBLE, i, 4, MPI_COMM_WORLD,
							MPI_STATUS_IGNORE);

					// Create a Point with the concentration[iCluster] as the value
					// and add it to myPoints
					xolotlViz::Point aPoint;
					aPoint.value = conc; // He
					aPoint.t = time;
					aPoint.x = x;
					myPoints[j].push_back(aPoint);
				}
			}
		}

		// Get all the reactants to have access to their names
		auto reactants = network->getAll();

		for (int i = 0; i < loopSize; i++) {
			auto cluster = (PSICluster *) reactants->at(i);
			// Get the data provider and give it the points
			auto thePoints = std::make_shared<std::vector<xolotlViz::Point> >(
					myPoints[i]);
			seriesPlot1D->getDataProvider(i)->setPoints(thePoints);
			seriesPlot1D->getDataProvider(i)->setDataName(cluster->getName());
		}

		// Change the title of the plot
		std::stringstream title;
		title << "Concentrations";
		seriesPlot1D->plotLabelProvider->titleLabel = title.str();
		// Give the time to the label provider
		std::stringstream timeLabel;
		timeLabel << "time: " << std::setprecision(4) << time << "s";
		seriesPlot1D->plotLabelProvider->timeLabel = timeLabel.str();
		// Get the current time step
		PetscReal currentTimeStep;
		ierr = TSGetTimeStep(ts, &currentTimeStep);CHKERRQ(ierr);
		// Give the timestep to the label provider
		std::stringstream timeStepLabel;
		timeStepLabel << "dt: " << std::setprecision(4) << currentTimeStep
				<< "s";
		seriesPlot1D->plotLabelProvider->timeStepLabel = timeStepLabel.str();

		// Render and save in file
		std::stringstream fileName;
		fileName << "log_series_TS" << timestep << ".pnm";
		seriesPlot1D->write(fileName.str());
	}

	else {
		// Send the value of the local grid size to the master process
		MPI_Send(&xm, 1, MPI_DOUBLE, 0, 4, MPI_COMM_WORLD);

		// Loop on the grid
		for (xi = xs; xi < xs + xm; xi++) {
			// Dump x
			x = grid[xi];

			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[xi];

			// Send the value of the local position to the master process
			MPI_Send(&x, 1, MPI_DOUBLE, 0, 4, MPI_COMM_WORLD);

			for (int i = 0; i < loopSize; i++) {
				// Send the value of the concentrations to the master process
				MPI_Send(&gridPointSolution[i], 1, MPI_DOUBLE, 0, 4,
						MPI_COMM_WORLD);
			}
		}
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "monitorSurface1D")
/**
 * This is a monitoring method that will save 2D plots for each depths of
 * the concentration as a function of the cluster composition.
 */
PetscErrorCode monitorSurface1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, xi;

	PetscFunctionBeginUser;

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network
	auto network = solverHandler->getNetwork();

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// Get the maximum size of HeV clusters
	auto psiNetwork = (PSIClusterReactionNetwork *) network;
	std::map<std::string, std::string> props = psiNetwork->getProperties();
	int maxHeVClusterSize = std::stoi(props["maxHeVClusterSize"]);
	int maxVClusterSize = std::stoi(props["maxVClusterSize"]);

	// Loop on the grid points
	for (xi = xs; xi < xs + xm; xi++) {
		// Create a Point vector to store the data to give to the data provider
		// for the visualization
		auto myPoints = std::make_shared<std::vector<xolotlViz::Point> >();

		// Get the pointer to the beginning of the solution data for this grid point
		gridPointSolution = solutionArray[xi];

		// A pointer for the clusters used below
		PSICluster * cluster;

		// Loop on Y = V number
		for (int i = 0; i <= maxVClusterSize; i++) {
			// Loop on X = He number
			for (int j = 0; j <= maxHeVClusterSize - maxVClusterSize; j++) {
				double conc = 0.0;
				// V clusters
				if (j == 0) {
					cluster = (PSICluster *) network->get("V",
							i);
					if (cluster) {
						// Get the ID of the cluster
						int id = cluster->getId() - 1;
						conc = gridPointSolution[id];
					}
				}
				// He clusters
				else if (i == 0) {
					cluster = (PSICluster *) network->get(
							"He", j);
					if (cluster) {
						// Get the ID of the cluster
						int id = cluster->getId() - 1;
						conc = gridPointSolution[id];
					}
				}
				// HeV clusters
				else {
					cluster =
							(PSICluster *) network->getCompound(
									"HeV", { j, i, 0 });
					if (cluster) {
						// Get the ID of the cluster
						int id = cluster->getId() - 1;
						conc = gridPointSolution[id];
					}
				}

				// Create a Point with the concentration as the value
				// and add it to myPoints
				xolotlViz::Point aPoint;
				aPoint.value = conc;
				aPoint.t = time;
				aPoint.x = j;
				aPoint.y = i;
				myPoints->push_back(aPoint);
			}
		}

		// Get the data provider and give it the points
		surfacePlot1D->getDataProvider()->setPoints(myPoints);
		surfacePlot1D->getDataProvider()->setDataName("brian");

		// Change the title of the plot
		std::stringstream title;
		title << "Concentration at Depth: " << grid[xi] << " nm";
		surfacePlot1D->plotLabelProvider->titleLabel = title.str();
		// Give the time to the label provider
		std::stringstream timeLabel;
		timeLabel << "time: " << std::setprecision(4) << time << "s";
		surfacePlot1D->plotLabelProvider->timeLabel = timeLabel.str();
		// Get the current time step
		PetscReal currentTimeStep;
		ierr = TSGetTimeStep(ts, &currentTimeStep);CHKERRQ(ierr);
		// Give the timestep to the label provider
		std::stringstream timeStepLabel;
		timeStepLabel << "dt: " << std::setprecision(4) << currentTimeStep
				<< "s";
		surfacePlot1D->plotLabelProvider->timeStepLabel = timeStepLabel.str();

		// Render and save in file
		std::stringstream fileName;
		fileName << "Brian_TS" << timestep << "_D" << xi << ".pnm";
		surfacePlot1D->write(fileName.str());
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "monitorMeanSize1D")
/**
 * This is a monitoring method that will create files with the mean
 * helium size as a function of depth at each time step.
 */
PetscErrorCode monitorMeanSize1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, xi;
	double x = 0.0;

	PetscFunctionBeginUser;

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network
	auto network = solverHandler->getNetwork();

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// Create the output file
	std::ofstream outputFile;
	if (procId == 0) {
		std::stringstream name;
		name << "heliumSizeMeanBis_" << timestep << ".dat";
		outputFile.open(name.str());
	}

	// Loop on the full grid
	for (xi = 0; xi < grid.size(); xi++) {
		// Wait for everybody at each grid point
		MPI_Barrier(PETSC_COMM_WORLD);

		// Get the x position
		x = grid[xi];

		// Initialize the values to write in the file
		double heliumMean = 0.0, standardDev = 0.0;

		// If this is the locally owned part of the grid
		if (xi >= xs && xi < xs + xm) {
			// Compute the mean and standard deviation of helium cluster size

			// Get the pointer to the beginning of the solution data for this grid point
			gridPointSolution = solutionArray[xi];

			// A pointer for the clusters used below
			PSICluster * cluster;
			// Initialize the total helium and concentration before looping
			double concTot = 0.0, heliumTot = 0.0;

			// Loop on all the indices to compute the mean
			for (int i = 0; i < heIndices1D.size(); i++) {
				concTot += gridPointSolution[heIndices1D[i]];
				heliumTot += gridPointSolution[heIndices1D[i]] * heWeights1D[i];
			}

			// Compute the mean size of helium at this depth
			heliumMean = heliumTot / concTot;

			// Initialize the standard deviation
			standardDev = 0.0;
			double deviation = 0.0;

			// Loop on all the indices to compute the standard deviation
			for (int i = 0; i < heIndices1D.size(); i++) {
				deviation = (heWeights1D[i] - heliumMean);
				standardDev += deviation * deviation * gridPointSolution[heIndices1D[i]];
			}

			// Compute the standard deviation at this depth
			standardDev = standardDev / concTot;
			standardDev = sqrt(standardDev);

			// If this is not the master process, send the values
			if (procId != 0) {
				MPI_Send(&heliumMean, 1, MPI_DOUBLE, 0, 5, MPI_COMM_WORLD);
				MPI_Send(&standardDev, 1, MPI_DOUBLE, 0, 5, MPI_COMM_WORLD);
			}
		}
		// If this process is not in charge of xi but is the master one, receive the value
		else if (procId == 0) {
			MPI_Recv(&heliumMean, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 5, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);
			MPI_Recv(&standardDev, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 5, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);
		}

		// The master process writes computes the cumulative value and writes in the file
		if (procId == 0) {
			outputFile << x << " " << heliumMean << " "
					<< standardDev << std::endl;
		}
	}

	// Close the file
	if (procId == 0) {
		outputFile.close();
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "monitorMaxClusterConc1D")
PetscErrorCode monitorMaxClusterConc1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, xi;

	PetscFunctionBeginUser;

	// Don't do anything if it was already printed
	if (!printMaxClusterConc1D) PetscFunctionReturn(0);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network
	auto network = solverHandler->getNetwork();

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// Get the maximum size of HeV clusters
	auto psiNetwork = (PSIClusterReactionNetwork *) network;
	std::map<std::string, std::string> props = psiNetwork->getProperties();
	int maxHeVClusterSize = std::stoi(props["maxHeVClusterSize"]);
	// Get the maximum size of V clusters
	int maxVClusterSize = std::stoi(props["maxVClusterSize"]);
	// Get the number of He in the max HeV cluster
	int maxHeSize = (maxHeVClusterSize - maxVClusterSize);
	// Get the maximum stable HeV cluster
	PSICluster * maxHeV;
	maxHeV = (PSICluster *) network->getCompound(
			"HeV", { maxHeSize, maxVClusterSize, 0 });

	// Boolean to know if the concentration is too big
	bool maxHeVTooBig = false;

	// Check the concentration of the biggest cluster at each grid point
	for (xi = xs; xi < xs + xm; xi++) {
		// Get the pointer to the beginning of the solution data for this grid point
		gridPointSolution = solutionArray[xi];

		// Get the concentration of the maximum HeV cluster
		auto maxHeVConc = gridPointSolution[maxHeV->getId() - 1];

		if (maxHeVConc > 1.0e-16) maxHeVTooBig = true;
	}

	// Slaves send the information about maxHeVTooBig to the main
	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);
	// Get the current process ID
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Main process
	if (procId == 0) {
		// Loop on the other processes
		for (int i = 1; i < worldSize; i++) {
			int otherBool;
			MPI_Recv(&otherBool, 1, MPI_INT, i, 6, MPI_COMM_WORLD,
					MPI_STATUS_IGNORE);

			// Update maxHeVTooBig
			maxHeVTooBig = maxHeVTooBig || otherBool;
		}

		// Print if maxHeVTooBig is true
		if (maxHeVTooBig) {
			std::cout << std::endl;
			std::cout << "At time step: " << timestep << " and time: " << time
					<< " the biggest cluster: " << *maxHeV
					<< " reached a concentration above 1.0e-16 at at least one grid point."
					<< std::endl << std::endl;

			// Don't print anymore
			printMaxClusterConc1D = false;
		}

		// Send this information to the other processes
		for (int i = 1; i < worldSize; i++) {
			int printMaxClusterConcInt = (int) printMaxClusterConc1D;
			MPI_Send(&printMaxClusterConcInt, 1, MPI_INT, i, 6, MPI_COMM_WORLD);
		}
	}
	// Other processes
	else {
		// Send the maxHeVTooBig value
		int maxHeVTooBigInt = (int) maxHeVTooBig;
		MPI_Send(&maxHeVTooBigInt, 1, MPI_INT, 0, 6, MPI_COMM_WORLD);

		// Receive the printMaxClusterConc1D value
		MPI_Recv(&printMaxClusterConc1D, 1, MPI_INT, 0, 6, MPI_COMM_WORLD,
				MPI_STATUS_IGNORE);
	}

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ Actual__FUNCT__("xolotlSolver", "monitorInterstitial1D")
/**
 * This is a monitoring method that will compute the flux of interstitials
 * at the surface
 */
PetscErrorCode monitorInterstitial1D(TS ts, PetscInt timestep, PetscReal time,
		Vec solution, void *ictx) {
	PetscErrorCode ierr;
	const double **solutionArray, *gridPointSolution;
	int xs, xm, xi;

	PetscFunctionBeginUser;

	// Get the number of processes
	int worldSize;
	MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);

	// Gets the process ID
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get the da from ts
	DM da;
	ierr = TSGetDM(ts, &da);CHKERRQ(ierr);

	// Get the solutionArray
	ierr = DMDAVecGetArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

	// Get the corners of the grid
	ierr = DMDAGetCorners(da, &xs, NULL, NULL, &xm, NULL, NULL);CHKERRQ(ierr);

	// Return if the grid point 1 is not on this process
	if (xs > 1) PetscFunctionReturn(0);

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network
	auto network = solverHandler->getNetwork();

	// Get the physical grid
	auto grid = solverHandler->getXGrid();

	// Get the concentrations at xi = 1
	xi = 1;
	gridPointSolution = solutionArray[xi];

	// Get the delta time from the previous timestep to this timestep
	double dt = time - previousTime;

	// Compute the total density of intersitials that escaped from the
	// surface since last timestep using the stored flux
	double nInterstitial = previousIFlux1D * dt;

//	// Uncomment to write the interstitial flux in a file
//	std::ofstream outputFile;
//	outputFile.open("interstitialOut.txt", ios::app);
//	outputFile << time << " "
//			<< nInterstitial << std::endl;
//	outputFile.close();

	// Initialize the value for the flux
	double newFlux = 0.0;

	// Get all the interstitial clusters
	auto interstitials = network->getAll("I");
	// Loop on them
	for (int i = 0; i < interstitials.size(); i++) {
		// Get the cluster
		auto cluster = (PSICluster *) interstitials.at(i);
		// Get its id and concentration
		int id = cluster->getId() - 1;
		double conc = gridPointSolution[id];
		// Get its size and diffusion coefficient
		int size = cluster->getSize();
		double coef = cluster->getDiffusionCoefficient();

		// Factor for finite difference
		double hxLeft = grid[xi] - grid[xi-1];
		double hxRight = grid[xi+1] - grid[xi];
		double factor = 2.0 / (hxLeft * (hxLeft + hxRight));
		// Compute the flux going to the left
		newFlux += (double) size * factor * coef * conc;
	}

	previousIFlux1D = newFlux;

	// Restore the solutionArray
	ierr = DMDAVecRestoreArrayDOFRead(da, solution, &solutionArray);CHKERRQ(ierr);

    PetscFunctionReturn(0);
}

/**
 * This operation sets up a monitor that will call monitorSolve
 * @param ts The time stepper
 * @return A standard PETSc error code
 */
PetscErrorCode setupPetsc1DMonitor(TS ts) {
	PetscErrorCode ierr;

	// Get the process ID
	int procId;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

	// Get xolotlViz handler registry
	auto vizHandlerRegistry = xolotlFactory::getVizHandlerRegistry();

	// Flags to launch the monitors or not
	PetscBool flag2DPlot, flag1DPlot, flagSeries, flagPerf, flagRetention,
			flagStatus, flagMaxClusterConc, flagInterstitial, flagCumul,
			flagMeanSize;

	// Check the option -plot_perf
	ierr = PetscOptionsHasName(NULL, "-plot_perf", &flagPerf);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-plot_perf) failed.");

	// Check the option -plot_series
	ierr = PetscOptionsHasName(NULL, "-plot_series", &flagSeries);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-plot_series) failed.");

	// Check the option -plot_1d
	ierr = PetscOptionsHasName(NULL, "-plot_1d", &flag1DPlot);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-plot_1d) failed.");

	// Check the option -plot_2d
	ierr = PetscOptionsHasName(NULL, "-plot_2d", &flag2DPlot);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-plot_2d) failed.");

	// Check the option -helium_retention
	ierr = PetscOptionsHasName(NULL, "-helium_retention", &flagRetention);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-helium_retention) failed.");

	// Check the option -start_stop
	ierr = PetscOptionsHasName(NULL, "-start_stop", &flagStatus);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-start_stop) failed.");

	// Check the option -max_cluster_conc
	ierr = PetscOptionsHasName(NULL, "-max_cluster_conc", &flagMaxClusterConc);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-max_cluster_conc) failed.");

	// Check the option -interstitial_diff
	ierr = PetscOptionsHasName(NULL, "-interstitial_diff", &flagInterstitial);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-interstitial_diff) failed.");

	// Check the option -helium_cumul
	ierr = PetscOptionsHasName(NULL, "-helium_cumul", &flagCumul);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-helium_cumul) failed.");

	// Check the option -mean_size
	ierr = PetscOptionsHasName(NULL, "-mean_size", &flagMeanSize);
	checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsHasName (-mean_size) failed.");

	// Get the solver handler
	auto solverHandler = PetscSolver::getSolverHandler();

	// Get the network and its size
	auto network = solverHandler->getNetwork();
	const int networkSize = network->size();

	// Set the monitor to save 1D plot of one concentration
	if (flag1DPlot) {
		// Only the master process will create the plot
		if (procId == 0) {
			// Create a ScatterPlot
			scatterPlot1D = vizHandlerRegistry->getPlot("scatterPlot1D",
					xolotlViz::PlotType::SCATTER);

			// Create and set the label provider
			auto labelProvider = std::make_shared<xolotlViz::LabelProvider>(
					"labelProvider");
			labelProvider->axis1Label = "x Position on the Grid";
			labelProvider->axis2Label = "Concentration";

			// Give it to the plot
			scatterPlot1D->setLabelProvider(labelProvider);

			// Create the data provider
			auto dataProvider = std::make_shared<xolotlViz::CvsXDataProvider>(
					"dataProvider");

			// Give it to the plot
			scatterPlot1D->setDataProvider(dataProvider);
		}

		// monitorSolve will be called at each timestep
		ierr = TSMonitorSet(ts, monitorScatter1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorScatter1D) failed.");
	}

	// Set the monitor to save 1D plot of many concentrations
	if (flagSeries) {
		// Only the master process will create the plot
		if (procId == 0) {
			// Create a ScatterPlot
			seriesPlot1D = vizHandlerRegistry->getPlot("seriesPlot1D",
					xolotlViz::PlotType::SERIES);

			// set the log scale
			seriesPlot1D->setLogScale();

			// Create and set the label provider
			auto labelProvider = std::make_shared<xolotlViz::LabelProvider>(
					"labelProvider");
			labelProvider->axis1Label = "x Position on the Grid";
			labelProvider->axis2Label = "Concentration";

			// Give it to the plot
			seriesPlot1D->setLabelProvider(labelProvider);

			// To plot a maximum of 18 clusters of the whole benchmark
			const int loopSize = std::min(18, networkSize);

			// Create a data provider for each cluster in the network
			for (int i = 0; i < loopSize; i++) {
				// Set the name for Identifiable
				std::stringstream dataProviderName;
				dataProviderName << "dataprovider" << i;
				// Create the data provider
				auto dataProvider = std::make_shared<xolotlViz::CvsXDataProvider>(
						dataProviderName.str());

				// Give it to the plot
				seriesPlot1D->addDataProvider(dataProvider);
			}
		}

		// monitorSeries1D will be called at each timestep
		ierr = TSMonitorSet(ts, monitorSeries1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorSeries1D) failed.");
	}

	// Set the monitor to save surface plots of clusters concentration
	// for each depth
	if (flag2DPlot) {
		// Create a SurfacePlot
		surfacePlot1D = vizHandlerRegistry->getPlot("surfacePlot1D",
				xolotlViz::PlotType::SURFACE);

		// Create and set the label provider
		auto labelProvider = std::make_shared<xolotlViz::LabelProvider>(
				"labelProvider");
		labelProvider->axis1Label = "He number";
		labelProvider->axis2Label = "V number";
		labelProvider->axis3Label = "Concentration";

		// Give it to the plot
		surfacePlot1D->setLabelProvider(labelProvider);

		// Create the data provider
		auto dataProvider = std::make_shared<xolotlViz::CvsXYDataProvider>(
				"dataProvider");

		// Give it to the plot
		surfacePlot1D->setDataProvider(dataProvider);

		// monitorSurface1D will be called at each timestep
		ierr = TSMonitorSet(ts, monitorSurface1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorSurface1D) failed.");
	}

	// Set the monitor to save performance plots (has to be in parallel)
	if (flagPerf) {
		// Only the master process will create the plot
		if (procId == 0) {
			// Create a ScatterPlot
			perfPlot = vizHandlerRegistry->getPlot("perfPlot",
					xolotlViz::PlotType::SCATTER);

			// Create and set the label provider
			auto labelProvider = std::make_shared<xolotlViz::LabelProvider>(
					"labelProvider");
			labelProvider->axis1Label = "Process ID";
			labelProvider->axis2Label = "Solver Time";

			// Give it to the plot
			perfPlot->setLabelProvider(labelProvider);

			// Create the data provider
			auto dataProvider = std::make_shared<xolotlViz::CvsXDataProvider>(
					"dataProvider");

			// Give it to the plot
			perfPlot->setDataProvider(dataProvider);
		}

		// monitorPerf will be called at each timestep
		ierr = TSMonitorSet(ts, monitorPerf, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorPerf) failed.");
	}

	// Initialize heIndices1D and heWeights1D if we want to compute the
	// retention or the cumulative value
	if (flagRetention || flagCumul || flagMeanSize) {
		// Get all the helium clusters
		auto heClusters = network->getAll(heType);

		// Get all the helium-vacancy clusters
		auto heVClusters = network->getAll(heVType);

		// Loop on the helium clusters
		for (int i = 0; i < heClusters.size(); i++) {
			auto cluster = (PSICluster *) heClusters[i];
			int id = cluster->getId() - 1;
			// Add the Id to the vector
			heIndices1D.push_back(id);
			// Add the number of heliums of this cluster to the weight
			heWeights1D.push_back(cluster->getSize());
		}

		// Loop on the helium-vacancy clusters
		for (int i = 0; i < heVClusters.size(); i++) {
			auto cluster = (PSICluster *) heVClusters[i];
			int id = cluster->getId() - 1;
			// Add the Id to the vector
			heIndices1D.push_back(id);
			// Add the number of heliums of this cluster to the weight
			auto comp = cluster->getComposition();
			heWeights1D.push_back(comp[heType]);
		}
	}

	// Set the monitor to compute the helium fluence and the retention
	// for the retention calculation
	if (flagRetention) {
		if (heIndices1D.size() == 0) {
			throw std::string(
					"PetscSolver Exception: Cannot compute the retention because "
					"there is no helium or helium-vacancy cluster in the network.");
		}

		// computeHeliumFluence will be called at each timestep
		ierr = TSMonitorSet(ts, computeHeliumFluence, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (computeHeliumFluence) failed.");

		// computeHeliumRetention1D will be called at each timestep
		ierr = TSMonitorSet(ts, computeHeliumRetention1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (computeHeliumRetention1D) failed.");

		// Uncomment to clear the file where the retention will be written
		std::ofstream outputFile;
		outputFile.open("retentionOut.txt");
		outputFile.close();
	}

	// Set the monitor to compute the cumulative helium concentration
	if (flagCumul) {
		if (heIndices1D.size() == 0) {
			throw std::string(
					"PetscSolver Exception: Cannot compute the cumulative "
					"concentration because there is no helium or helium-vacancy "
					"cluster in the network.");
		}

		// computeCumulativeHelium1D will be called at each timestep
		ierr = TSMonitorSet(ts, computeCumulativeHelium1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (computeCumulativeHelium1D) failed.");
	}

	// Set the monitor to save text file of the mean helium size
	if (flagMeanSize) {
		// monitorMeanSize1D will be called at each timestep
		ierr = TSMonitorSet(ts, monitorMeanSize1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorMeanSize1D) failed.");
	}

	// Set the monitor to save the status of the simulation in hdf5 file
	if (flagStatus) {
		// Find the stride to know how often the HDF5 file has to be written
		PetscBool flag;
		ierr = PetscOptionsGetInt(NULL, "-start_stop", &hdf5Stride1D, &flag);
		checkPetscError(ierr, "setupPetsc1DMonitor: PetscOptionsGetInt (-start_stop) failed.");
		if (!flag)
			hdf5Stride1D = 1;

		PetscInt Mx;
		PetscErrorCode ierr;

		// Get the da from ts
		DM da;
		ierr = TSGetDM(ts, &da);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSGetDM failed.");

		// Get the size of the total grid
		ierr = DMDAGetInfo(da, PETSC_IGNORE, &Mx, PETSC_IGNORE, PETSC_IGNORE,
		PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE,
		PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE, PETSC_IGNORE,
		PETSC_IGNORE);
		checkPetscError(ierr, "setupPetsc1DMonitor: DMDAGetInfo failed.");

		// Initialize the HDF5 file for all the processes
		xolotlCore::HDF5Utils::initializeFile(hdf5OutputName1D, networkSize);

		// Get the solver handler
		auto solverHandler = PetscSolver::getSolverHandler();

		// Get the physical grid
		auto grid = solverHandler->getXGrid();

		// Save the header in the HDF5 file
		xolotlCore::HDF5Utils::fillHeader(1, Mx, grid[1] - grid[0]);

		// Save the network in the HDF5 file
		xolotlCore::HDF5Utils::fillNetwork(network);

		// Finalize the HDF5 file
		xolotlCore::HDF5Utils::finalizeFile();

		// startStop1D will be called at each timestep
		ierr = TSMonitorSet(ts, startStop1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (startStop1D) failed.");
	}

	// Set the monitor to output information about when the maximum stable HeV
	// cluster in the network first becomes greater than 1.0e-16
	if (flagMaxClusterConc) {
		// monitorMaxClusterConc1D will be called at each timestep
		ierr = TSMonitorSet(ts, monitorMaxClusterConc1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorMaxClusterConc1D) failed.");
	}

	// Set the monitor on the outgoing flux of interstitials at the surface
	if (flagInterstitial) {
		// monitorInterstitial1D will be called at each timestep
		ierr = TSMonitorSet(ts, monitorInterstitial1D, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorInterstitial1D) failed.");

//		// Uncomment to clear the file where the interstitial flux will be written
//		std::ofstream outputFile;
//		outputFile.open("interstitialOut.txt");
//		outputFile.close();
	}

	// Set the monitor to simply change the previous time to the new time
	if (flagRetention || flagInterstitial) {
		// monitorTime will be called at each timestep
		ierr = TSMonitorSet(ts, monitorTime, NULL, NULL);
		checkPetscError(ierr, "setupPetsc1DMonitor: TSMonitorSet (monitorTime) failed.");
	}

	PetscFunctionReturn(0);
}

}

/* end namespace xolotlSolver */