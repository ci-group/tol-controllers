#include "CppnGenome.h"


CppnGenome::CppnGenome(int size) : size(size)
{
    //Create a new genome.
    std::vector<NEAT::GeneticNodeGene> nodeVector;
    
    //The bias node. The activation function is linear so
    //It will simply output its value.
    nodeVector.push_back(NEAT::GeneticNodeGene(
                            INPUT_BIAS,
                            TYPE_INPUT,
                            DRAW_POS_ZERO,
                            RANDOM_ACTIVATION_FALSE,
                            ACTIVATION_FUNCTION_LINEAR)
                         );
    
    //The input nodes for the x and y values for the activation value matrix
    //Random activation is false here, but it might be better to make it true
    nodeVector.push_back(NEAT::GeneticNodeGene(
                            INPUT_X,
                            TYPE_INPUT,
                            0,
                            DRAW_POS_ZERO,
                            RANDOM_ACTIVATION_TRUE)
                         );
    nodeVector.push_back(NEAT::GeneticNodeGene(
                            INPUT_Y,
                            TYPE_INPUT,
                            0,
                            DRAW_POS_ZERO,
                            RANDOM_ACTIVATION_TRUE)
                         );
    
    //The output node of the network.
    nodeVector.push_back(NEAT::GeneticNodeGene(
                            OUTPUT,
                            TYPE_OUTPUT,
                            0,
                            DRAW_POS_HALF_MAX,
                            RANDOM_ACTIVATION_TRUE)
                         );
    
    //The creation of the new genome.
    //New individuals will start with a 'fully' connected network:
    //the input nodes will be connected to the output node.
    //Handled by smart pointer
    cppn = boost::shared_ptr<NEAT::GeneticIndividual>(new NEAT::GeneticIndividual(
                                                            nodeVector,
                                                            CREATE_TOPOLOGY_TRUE,
                                                            EDGE_DENSITY_FULLY_CONNECTED)
                                                      );
}


CppnGenome::CppnGenome(const CppnGenome& genome1) : size(genome1.size), cppn(genome1.cppn)
{
    
}


CppnGenome::CppnGenome(std::istream& stream)
{
    std::string genomeStr;
    std::string cppnStr;
    
    stream >> genomeStr >> size >> cppnStr;
    cppn = boost::shared_ptr<NEAT::GeneticIndividual>(new NEAT::GeneticIndividual(stream));
}


CppnGenome::~CppnGenome()
{
    
}


string CppnGenome::toString() const
{
    std::string result = std::string("genome ");
    result.append(std::to_string(size));
    result.append(" cppn ");
    
    ostringstream oss;
    cppn->dump(oss);
    result.append(oss.str());
    
    return result;
}


void CppnGenome::mutate()
{
    cppn = boost::shared_ptr<NEAT::GeneticIndividual>(new NEAT::GeneticIndividual(cppn, true));
    mutateSize();
}


void CppnGenome::crossoverAndMutate(const CppnGenome& genome)
{
    cppn = boost::shared_ptr<NEAT::GeneticIndividual>(new NEAT::GeneticIndividual(cppn, genome.getCppn(), true));
    
    //The random between 0 and 1 ensures that a crossover of, for example,
    //5 and 6 will result in 5 only 50% of the time, and 6 the other 50%.
    //Not using it would give a bias to the smaller parent.
    size = (size + genome.getSize() + RANDOM.getRandomDouble()) / 2.0;
    mutateSize();
}


boost::shared_ptr<NEAT::GeneticIndividual> CppnGenome::getCppn() const
{
    return cppn;
}


double CppnGenome::getSize() const
{
    return size;
}


void CppnGenome::mutateSize()
{
    if (RANDOM.getRandomDouble() < SIZE_MUTATION_RATE)
    {
//        std::default_random_engine generator;
//        std::normal_distribution<double> distribution(0,SIZE_MUTATION_STRENGTH);
//        std::binomial_distribution<int> distribution(2,0.5);
//        double randomMutation = distribution(generator);
        
        int randomMutation = RANDOM.getRandomWithinRange(-1, 1);
        size = size + randomMutation;
        if(size < CPPN_GRID_MINIMUM_SIZE) {
            size = CPPN_GRID_MINIMUM_SIZE;
        }
    }
}