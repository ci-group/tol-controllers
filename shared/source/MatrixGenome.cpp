//
//  MatrixGenome.cpp
//  shared
//
//  Created by Berend Weel on 16/05/14.
//  Copyright (c) 2014 Berend Weel. All rights reserved.
//
#include <sstream>

#include "MatrixGenome.h"


MatrixGenome::MatrixGenome() {
    x_size = 0;
    y_size = 0;
    
    random = Utils::Random::getInstance();
}

/**
 * Constructs a random, minimal genome with a matrix of size_x by size_y.
 * The minimal genome will contain one output node, two input nodes
 * and a bias node. The network will be fully connected and all
 * nodes, except the bias node, will have a random activation function.
 *
 * @param size_x The x size of this genome.
 * @param size_y The y size of this genome.
 */
MatrixGenome::MatrixGenome(int size_x, int size_y) {
    x_size = size_x;
    y_size = size_y;
    
    genes = std::vector<std::vector<double> >(x_size,std::vector<double>(size_y,0));
    
    random = Utils::Random::getInstance();
    
    for(int i=0;i<x_size;i++){
        for(int j=0;j<y_size;j++){
            genes[i][j] = random->normal_real();
        }
    }
}

/**
 * Constructs a copy of genome1.
 * The genome will not be mutated.
 *
 * @param genome1 The genome to be copied.
 */
MatrixGenome::MatrixGenome(const MatrixGenome& genome1) {
    x_size = genome1.x_size;
    y_size = genome1.y_size;
    
    genes = genome1.genes;
    
    random = Utils::Random::getInstance();
}

/**
 * Constructs the genome described by stream.
 * This constructor is able to process streams based on the output of the toString() function,
 * creating the exact same genome that returned the string.
 *
 * @param stream Stream containing the description of a CppnGenome.
 */
MatrixGenome::MatrixGenome(std::istream& stream) {
    std::string genomeStr;
    std::string valuesStr;
    
    random = Utils::Random::getInstance();
    
    stream >> genomeStr >> x_size >> y_size >> valuesStr;
    
    genes.resize(x_size);
    for(int i=0;i<x_size;i++) {
        genes[i].resize(y_size);
        for(int j=0;j<y_size;j++){
            stream >> genes[i][j];
        }
    }
}

/**
 * Sets the parameters of the genome.
 * This overwrites any values that have been set before.
 *
 * @param The values
 */
void MatrixGenome::setValues(std::vector<std::vector<double> > values) {
    genes = values;
    
    x_size = values.size();
    if(x_size > 0 ){
        y_size = values[0].size();
    }else{
        y_size = 0;
    }
}

/**
 * Destroys the CppnGenome.
 * Does not actually perform any additional clean-up.
 */
MatrixGenome::~MatrixGenome() {
    
}

/**
 * Clone implementation for the use of ptr_vector
 *
 */
MatrixGenome *MatrixGenome::clone () const {
    return new MatrixGenome(*this);
}

void MatrixGenome::readString(std::string genome){
    std::istringstream stream(genome);
    
    std::string genomeStr;
    std::string valuesStr;
    
    stream >> genomeStr >> x_size >> y_size >> valuesStr;
    
    genes.resize(x_size);
    for(int i=0;i<x_size;i++) {
        genes[i].resize(y_size);
        for(int j=0;j<y_size;j++){
            stream >> genes[i][j];
        }
    }
}

/**
 * Returns a string of this genome that contains enough information to create
 * an exact copy of this genome.
 * Used in combination with the stream based constructor to write the genome to file,
 * and then reconstruct for detailed analysis.
 * Useful for debugging, logging or saving genomes for later use.
 *
 * @return Returns a string containing all information to reconstruct the genome.
 */
std::string MatrixGenome::toString() const {
    std::string result = std::string("MATRIX ");
    result.append(std::to_string(x_size));
    result.append(" ");
    result.append(std::to_string(y_size));
    result.append(" VALUES ");
    
    std::ostringstream oss;
    for(int i=0;i<x_size;i++) {
        for(int j=0;j<y_size;j++){
            oss << genes[i][j] << " ";
        }
    }
    result.append(oss.str());
    
    return result;
}

/**
 * Mutates this genome.
 *
 * Mutation of happens with a chance of SIZE_MUTATION_RATE
 * by adding a random numbers drawn from a normal distribution with a mean of zero
 * and a standard deviation of SIZE_MUTATION_STRENGTH
 */
void MatrixGenome::mutate() {
    for (int i=0;i<x_size;i++){
        for(int j=0;j<y_size;j++){
            if (random->uniform_real() < MATRIX_MUTATION_RATE)
            {
                genes[i][j] += random->normal_real() * MATRIX_MUTATION_STRENGTH;
            }
        }
    }
}

/**
 * Combines this genome with the input genome and then mutates this genome.
 *
 * The size of the SMALLEST genome is used to prevent arbitrary values that
 * do not mean anything for the organism.
 *
 * Mutation of happens with a chance of SIZE_MUTATION_RATE
 * by adding random numbers drawn from a normal distribution with a mean of zero
 * and a standard deviation of SIZE_MUTATION_STRENGTH
 *
 * @param genome The genome to use in the crossover.
 */
void MatrixGenome::crossoverAndMutate(boost::shared_ptr<MindGenome> g) {
    
    MatrixGenome genome = *boost::dynamic_pointer_cast<MatrixGenome>(g);
    if(x_size > genome.x_size){
        x_size = genome.x_size;
        genes.resize(x_size);
    }
    
    if(y_size > genome.y_size){
        y_size = genome.y_size;
        for (int i=0;i<x_size;i++){
            genes[0].resize(y_size);
        }
    }
    
    // Uniform crossover
    for (int i=0;i<x_size;i++){
        for(int j=0;j<y_size;j++){
            if(random->uniform_real() < 0.5){
                genes[i][j] = genome.genes[i][j];
            }
        }
    }
    mutate();
}

/**
 * Returns a copy of the matrix
 *
 * @return a copy of the matrix
 */
std::vector<std::vector<double> > MatrixGenome::getMatrix() {
    return genes;
}

/**
 * Returns the x size of the matrix,
 *
 * @return Returns the size of the Activation Value Matrix.
 */
int MatrixGenome::getSizeX() const {
    return x_size;
}

/**
 * Returns the y size of the matrix
 *
 * @return Returns the size of the Activation Value Matrix.
 */
int MatrixGenome::getSizeY() const {
    return y_size;
}