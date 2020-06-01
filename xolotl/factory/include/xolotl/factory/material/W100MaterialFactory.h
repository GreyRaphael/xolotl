#ifndef W100MATERIALHANDLERFACTORY_H
#define W100MATERIALHANDLERFACTORY_H

#include <memory>
#include <xolotl/factory/material/MaterialFactory.h>
#include <xolotl/core/flux/W100FitFluxHandler.h>
#include <xolotl/core/advection/W100AdvectionHandler.h>
#include <xolotl/core/modifiedreaction/trapmutation/W100TrapMutationHandler.h>

namespace xolotlFactory {

/**
 * Subclass of MaterialFactory for a (100) oriented tungsten material.
 */
class W100MaterialFactory: public MaterialFactory {
public:

	/**
	 * The constructor creates the handlers.
	 *
	 * @param dim The number of dimensions for the problem
	 */
	W100MaterialFactory(const xolotlCore::Options &options) :
			MaterialFactory(options) {
		theFluxHandler = std::make_shared<xolotlCore::W100FitFluxHandler>();
		theAdvectionHandler.push_back(
				std::make_shared<xolotlCore::W100AdvectionHandler>());
		theTrapMutationHandler = std::make_shared<
				xolotlCore::W100TrapMutationHandler>();
		theNucleationHandler = std::make_shared<
				xolotlCore::DummyNucleationHandler>();

		return;
	}

	/**
	 * The destructor
	 */
	~W100MaterialFactory() {
	}
};

} // end namespace xolotlFactory

#endif // W100MATERIALHANDLERFACTORY_H