#ifndef NECLUSTER_H
#define NECLUSTER_H

// Includes
#include <memory>
#include <sstream>
#include <cassert>
#include <Reactant.h>

namespace xolotlPerf {
class ITimer;
}

namespace xolotlCore {

/**
 * The NECluster class is a Reactant that is specialized to work for
 * simulations of fission materials. It provides special routines
 * for calculating the total flux due to production and dissociation and
 * obtaining the cluster size.
 *
 * NEClusters must always be initialized with a size. If the constructor is
 * passed a size of zero or less, the actual size will be set to 1.
 *
 * The getComposition() operation is implemented by subclasses and will always
 * return a map with the keys Xe, V, I, XeV or XeI. The operation getTypeName()
 * will always return one of the same values.
 *
 * As a rule, it is possible to access directly some of the private members of
 * this class (id, concentration, reactionRadius, diffusionCoefficient, size,
 * type) instead of using the "get" functions for performance reasons. In
 * order to change these values the "set" functions must still be used.
 */
class NECluster: public Reactant {

protected:

	/**
	 * This operation returns a set that contains only the entries of the
	 * reaction connectivity array that are non-zero.
	 *
	 * @return The set of connected reactants. Each entry in the set is the id
	 * of a connected cluster for forward reactions.
	 */
	std::set<int> getReactionConnectivitySet() const;

	/**
	 * This operation returns a set that contains only the entries of the
	 * dissociation connectivity array that are non-zero.
	 *
	 * @return The set of connected reactants. Each entry in the set is the id
	 * of a connected cluster for dissociation reactions
	 */
	const std::set<int> & getDissociationConnectivitySet() const;

	/**
	 * This operation returns the total change in this cluster due to
	 * other clusters dissociating into it.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] flux The flux due to dissociation of other clusters
	 */
	void getDissociationFlux(const double* concs, int i,
                            Reactant::Flux& flux) const override;

	/**
	 * This operation returns the total change in this cluster due its
	 * own dissociation.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] flux The flux due to its dissociation
	 */
	void getEmissionFlux(const double* concs, int i,
                            Reactant::Flux& flux) const override;

	/**
	 * This operation param[out] fluxs the total change in this cluster due to
	 * the production of this cluster by other clusters.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] flux The flux due to this cluster being produced
	 */
	void getProductionFlux(const double* concs, int i,
                            Reactant::Flux& flux) const override;

	/**
	 * This operation param[out] fluxs the total change in this cluster due to
	 * the combination of this cluster with others.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] flux The flux due to this cluster combining with other clusters
	 */
	void getCombinationFlux(const double* concs, int i,
                            Reactant::Flux& flux) const override;

public:

	/**
	 * This is a protected class that is used to implement the flux calculations
	 * for two body reactions or dissociation.
	 *
	 * The constant k+ or k- is stored along the clusters taking part in the
	 * reaction or dissociation for faster computation because they only change
	 * when the temperature change. k is computed when setTemperature() is called.
	 */
	class ClusterPair {
	public:

		/**
		 * The first cluster in the pair
		 */
		NECluster * first;

		/**
		 * The second cluster in the pair
		 */
		NECluster * second;

		/**
		 * The first cluster distance in the group (0.0 for non-super clusters)
		 */
		double firstDistance;

		/**
		 * The second cluster distance in the group (0.0 for non-super clusters)
		 */
		double secondDistance;

		/**
		 * The reaction/dissociation pointer to the list
		 */
		// NB: we use a reference_wrapper because we assign
		// this after constructing the object.
		// TODO why can't we add this when we construct the object?
		std::reference_wrapper<Reaction> reaction;

		//! The constructor
		ClusterPair(Reaction& _reaction, NECluster * firstPtr,
				NECluster * secondPtr) :
				reaction(_reaction), first(firstPtr), second(secondPtr), firstDistance(
						0.0), secondDistance(0.0) {
		}
	};

	/**
	 * This is a protected class that is used to implement the flux calculations
	 * for combinations.
	 *
	 * The constant k+ is stored along the cluster that combines with this cluster
	 * for faster computation because they only change when the temperature change.
	 * k+ is computed when setTemperature() is called.
	 */
	class CombiningCluster {
	public:

		/**
		 * The combining cluster
		 */
		// We use a reference wrapper because we may need to reassign
		// the original combining reactant to a super cluster after
		// construction.
		NECluster * combining;

		/**
		 * The reaction pointer to the list
		 */
		// We use a reference wrapper here because it allows NESuperCluster
		// to edit vectors of CombiningClusters in place when grouping
		// into superclusters.
		// TODO can't this be done similar to what we're doing in PSI
		// to avoid the need for the reference wrappers?
		std::reference_wrapper<Reaction> reaction;

		/**
		 * The cluster distance in the group (0.0 for non-super clusters)
		 */
		double distance;

		//! The constructor
		CombiningCluster(Reaction& _reaction, NECluster * _comb) :
				combining(_comb), reaction(_reaction), distance(0.0) {
		}
	};

	/**
	 * A vector of ClusterPairs that represents reacting pairs of clusters
	 * that produce this cluster. This vector should be populated early in the
	 * cluster's lifecycle by subclasses. In the standard Xolotl clusters,
	 * this vector is filled in createReactionConnectivity.
	 */
	std::vector<ClusterPair> reactingPairs;

	/**
	 * A vector of clusters that combine with this cluster to produce other
	 * clusters. This vector should be populated early in the cluster's
	 * lifecycle by subclasses. In the standard Xolotl clusters, this vector is
	 * filled in createReactionConnectivity.
	 */
	std::vector<CombiningCluster> combiningReactants;

	/**
	 * A vector of pairs of clusters: the first one is the one dissociation into
	 * this cluster, the second one is the one that is emitted at the same time
	 * during the dissociation. This vector should be populated early in the
	 * cluster's lifecycle by subclasses. In the standard Xolotl clusters, this
	 * vector is filled in dissociateCluster that is called by
	 * createDissociationConnectivity.
	 */
	std::vector<ClusterPair> dissociatingPairs;

	/**
	 * A vector of ClusterPairs that represent pairs of clusters that are emitted
	 * from the dissociation of this cluster. This vector should be populated early
	 * in the cluster's lifecycle by subclasses. In the standard Xolotl clusters,
	 * this vector is filled in emitClusters that is called by
	 * createDissociationConnectivity.
	 */
	std::vector<ClusterPair> emissionPairs;

	/**
	 * Default constructor, deleted because we require info to construct.
	 */
	NECluster() = delete;

	/**
	 * The default constructor
	 *
	 * @param _network The network to which we wil belong.
	 * @param _registry The performance handler registry.
	 * @param _name Our name.
	 */
	NECluster(IReactionNetwork& _network,
			std::shared_ptr<xolotlPerf::IHandlerRegistry> _registry,
			const std::string& _name = "NECluster") :
			Reactant(_network, _registry, _name) {
	}

	/**
	 * Copy constructor, deleted to prevent use.
	 */
	NECluster(NECluster &other) = delete;

	/**
	 * The destructor
	 */
	virtual ~NECluster() {
	}

	/**
	 * Update reactant using other reactants in its network.
	 */
	virtual void updateFromNetwork() override;

	/**
	 * Note that we result from the given reaction.
	 * Assumes the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void resultFrom(ProductionReaction& reaction, int a[4] = { },
			int b[4] = { }) override;

	/**
	 * Note that we result from the given reaction.
	 * Assumes the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void resultFrom(ProductionReaction& reaction, double *coef) override;

	/**
	 * Note that we combine with another cluster in a production reaction.
	 * Assumes that the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void participateIn(ProductionReaction& reaction, int a[4] = { }) override;

	/**
	 * Note that we combine with another cluster in a production reaction.
	 * Assumes that the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void participateIn(ProductionReaction& reaction, double *coef) override;

	/**
	 * Note that we combine with another cluster in a dissociation reaction.
	 * Assumes the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void participateIn(DissociationReaction& reaction, int a[4] = { },
			int b[4] = { }) override;

	/**
	 * Note that we combine with another cluster in a dissociation reaction.
	 * Assumes the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void participateIn(DissociationReaction& reaction, double *coef) override;

	/**
	 * Note that we emit from the given reaction.
	 * Assumes the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void emitFrom(DissociationReaction& reaction, int a[4] = { }) override;

	/**
	 * Note that we emit from the given reaction.
	 * Assumes the reaction is already in our network.
	 *
	 * \see Reactant.h
	 */
	void emitFrom(DissociationReaction& reaction, double *coef) override;

	/**
	 * Add the reactions to the network lists.
	 */
	virtual void optimizeReactions() override;

	/**
	 * This operation returns the connectivity array for this cluster for
	 * forward reactions. An entry with value one means that this cluster
	 * and the cluster with id = index + 1 are connected.
	 *
	 * @return The connectivity array for "forward" (non-dissociating)
	 * reactions
	 */
	virtual std::vector<int> getReactionConnectivity() const;

	/**
	 * This operation returns the connectivity array for this cluster for
	 * forward reactions. An entry with value one means that this cluster
	 * and the cluster with id = index + 1 are connected.
	 *
	 * @return The connectivity array for "forward" (non-dissociating)
	 * reactions
	 */
	virtual std::vector<int> getDissociationConnectivity() const;

	/**
	 * This operation returns the current concentration.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param distXe The xenon distance in the group
	 * @return The concentration of this reactant
	 */
    virtual double getConcentration(const double* concs, double distXe) const {
		// TODO should this ever be called?  It ignores the argument.
		return concs[id - 1];
    }

	/**
	 * Obtain current concentration.
	 *
	 * We hid the base class' zero-argument getConcentration when
	 * we defined the two-argument version here.  This tells the compiler
	 * to allow us to use the base class' zero-argument version also
	 * without having to explicitly specify the class scope when calling it.
	 *
	 * @return The concentration of the reactant.
	 */
	using Reactant::getConcentration;

	/**
	 * This operation returns the first xenon moment.
	 *
	 * @return The moment
	 */
	virtual double getMoment(const double* concs) const {
        return 0.0;
    }

	/**
	 * This operation returns the total flux of this cluster in the
	 * current network.
	 *
	 * @param i The location on the grid in the depth direction
	 * @return The total change in flux for this cluster due to all
	 * reactions
	 */
	double getTotalFlux(const double* concs, int i) override {
        // Compute total flux.
        auto flux = getTotalFluxHelper<Reactant::Flux>(concs, i);
        return flux.flux;
    }

	/**
	 * This operation works as getPartialDerivatives above, but instead of
	 * returning a vector that it creates it fills a vector that is passed to
	 * it by the caller. This allows the caller to optimize the amount of
	 * memory allocations to just one if they are accessing the partial
	 * derivatives many times.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] partials The vector that should be filled with the partial 
     * derivatives for this reactant where index zero corresponds to the 
     * first reactant in the list returned by the ReactionNetwork::getAll()
     * operation. The size of the vector should be equal to 
     * ReactionNetwork::size().
	 *
	 */
	virtual void getPartialDerivatives(const double* concs, int i,
            std::vector<double> & partials) const override;

	/**
	 * This operation computes the partial derivatives due to production
	 * reactions.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] partials The vector that should be filled with the partial 
     * derivatives for this reactant where index zero corresponds to the 
     * first reactant in the list returned by the ReactionNetwork::getAll()
     * operation. The size of the vector should be equal to 
     * ReactionNetwork::size().
	 */
	virtual void getProductionPartialDerivatives(const double* concs, int i,
            std::vector<double> & partials) const;

	/**
	 * This operation computes the partial derivatives due to combination
	 * reactions.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] partials The vector that should be filled with the partial 
     * derivatives for this reactant where index zero corresponds to the 
     * first reactant in the list returned by the ReactionNetwork::getAll()
     * operation. The size of the vector should be equal to 
     * ReactionNetwork::size().
	 */
	virtual void getCombinationPartialDerivatives(const double* concs, int i,
			std::vector<double> & partials) const;

	/**
	 * This operation computes the partial derivatives due to dissociation of
	 * other clusters into this one.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] partials The vector that should be filled with the partial 
     * derivatives for this reactant where index zero corresponds to the 
     * first reactant in the list returned by the ReactionNetwork::getAll()
     * operation. The size of the vector should be equal to 
     * ReactionNetwork::size().
	 */
	virtual void getDissociationPartialDerivatives(const double* concs, int i,
			std::vector<double> & partials) const;

	/**
	 * This operation computes the partial derivatives due to emission
	 * reactions.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The location on the grid in the depth direction
	 * @param[out] partials The vector that should be filled with the partial 
     * derivatives for this reactant where index zero corresponds to the 
     * first reactant in the list returned by the ReactionNetwork::getAll()
     * operation. The size of the vector should be equal to 
     * ReactionNetwork::size().
	 */
	virtual void getEmissionPartialDerivatives(const double* concs, int i,
            std::vector<double> & partials) const;

	/**
	 * This operation reset the connectivity sets based on the information
	 * in the production and dissociation vectors.
	 */
	void resetConnectivities() override;

	/**
	 * This operation returns the sum of combination rate and emission rate
	 * (where this cluster is on the left side of the reaction) for this
	 * particular cluster.
	 * This is used to computed the desorption rate in the
	 * modified trap-mutation handler.
	 *
     * @param concs Current solution vector for desired grid point.
	 * @param i The position on the grid
	 * @return The rate
	 */
	double getLeftSideRate(const double* concs, int i) const override;

	/**
	 * This operation returns the vector of production reactions in which
	 * this cluster is involved, containing the id of the reactants, and
	 * the distance.
	 *
	 * @return The vector of productions
	 */
	virtual std::vector<std::vector<double> > getProdVector() const override;

	/**
	 * This operation returns the vector of combination reactions in which
	 * this cluster is involved, containing the id of the other reactants, and
	 * the distance.
	 *
	 * @return The vector of combinations
	 */
	virtual std::vector<std::vector<double> > getCombVector() const override;

	/**
	 * This operation returns the vector of dissociation reactions in which
	 * this cluster is involved, containing the id of the emitting reactants, and
	 * the distance
	 *
	 * @return The vector of dissociations
	 */
	virtual std::vector<std::vector<double> > getDissoVector() const override;

	/**
	 * This operation returns the vector of emission reactions in which
	 * this cluster is involved.
	 *
	 * @return The vector of productions
	 */
	virtual std::vector<std::vector<double> > getEmitVector() const override;

	/**
	 * This operation returns a list that represents the connectivity
	 * between this cluster and other clusters in the network.
	 * "Connectivity" indicates whether two clusters interact, via any
	 * mechanism, in an abstract sense (as if they were nodes connected by
	 * an edge on a network graph).
	 *
	 * @return An array of ones and zeros that indicate whether or not this
	 * cluster interacts via any mechanism with another cluster. A "1" at
	 * the i-th entry in this array indicates that the cluster interacts
	 * with the i-th cluster in the ReactionNetwork and a "0" indicates
	 * that it does not.
	 */
	std::vector<int> getConnectivity() const override;

	/**
	 * Tell reactant to output a representation of its reaction coefficients
	 * to the given output stream.
	 *
	 * @param os Output stream on which to output coefficients.
	 */
	virtual void outputCoefficientsTo(std::ostream& os) const override {
		// NIY.
		assert(false);
	}
};

} /* end namespace xolotlCore */
#endif
