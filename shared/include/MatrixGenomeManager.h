//
//  MatrixGenomeManager.h
//  EvolverController
//
//  Created by Berend Weel on 16/05/14.
//  Copyright (c) 2014 Berend Weel. All rights reserved.
//

#ifndef EvolverController_MatrixGenomeManager_h
#define EvolverController_MatrixGenomeManager_h

#include <boost/shared_ptr.hpp>

#include "MindGenomeManager.h"
#include "MatrixGenome.h"

/**
 * Genome Manager implementation that uses genomes based on a matrix.

 * The Cppn Genome Manager stores all genomes it creates in a map,
 * and returns a unique id for the genome.
 * This genome id can then be used to obtain a BuildPlan for the genome,
 * create mutations or crossovers from the genomes,
 * obtain the genome as a string
 * or remove the genome from the map.
 */
class MatrixGenomeManager : public MindGenomeManager
{
	
public:
	
    /**
	 * Creates a new genome manager and sets NEAT global settings.
	 * The NEAT global singleton is currently also used to produce random numbers for the Organism Manager,
	 * meaning that the seed passed to the constructor will be the seed used for the entire Organism Manager.
	 * Also, creating a second genome manager will reset the global seed and will overwrite all NEAT settings.
	 *
	 * For a more flexible Genome Manager some parameters should be added to the constructor,
	 * or handled in a separate setParameter() function.
	 *
	 * @param seed The seed used by the NEAT global singleton and,
	 * as a result, the seed for all random numbers generated by the Organism Manager.
	 */
	MatrixGenomeManager(time_t seed = -1);
    
    /**
     * Destructs the Cppn Genome Manager.
     * Does not actually perform any additional clean-up.
     */
	virtual ~MatrixGenomeManager();
    
    /**
     * Creates a new genome based on the genome ids in the input vector,
     * adds the new genome to a genome map and returns an id for the genome.
     *
     * If the input vector is empty, the Cppn Genome Manager will create a genome with a
     * minimal cppn and a grid size of ORGANISM_STARTING_SIZE.
     * If the input vector contains one id, the Cppn Genome Manager will create a genome
     * that is a mutation of the genome associated with this id.
     * If the input vector contains two ids, the Cppn Genome Manager will create a genome
     * that is a crossover and mutation of the genomes associated with those ids.
     * If the input vector contains more than two ids, the Cppn Genome Manager will throw
     * an unsupported exception and exit.
     *
     * @param genomeIDs A vector containing the ids of the genomes that should be used as
     * a basis for creating the new genome.
     * @return Returns the id of the created genome.
     */
	MatrixGenome createGenome(const std::vector<MatrixGenome>& parentsGenomes);
    
    std::string genomeToString(MatrixGenome genome) const;
    
    std::string genomeArrayToString(boost::ptr_vector<MindGenome> genomes);
    
    boost::shared_ptr<MindGenome> getGenomeFromStream(std::istream& stream);
    
    virtual boost::ptr_vector<MindGenome> readStringToArray(std::string genomeArray);
    
    virtual boost::shared_ptr<MindGenome> createGenome(const std::vector<boost::shared_ptr<MindGenome> > & parentsGenomes);
};

#endif
