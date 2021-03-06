#include "Random.h"
#include "Defines.h"

namespace Utils
{
    Random* Random::instance = NULL;

	Random::Random()
	:
	_seeder(),
	_seed(_seeder()),
	_engine(_seed),
	_integer_dist(),
	_integer_gen(_engine, _integer_dist),
	_real_dist(),
	_real_gen(_engine, _real_dist),
	_normal_dist(),
	_normal_gen(_engine, _normal_dist),
	_cauchy_dist(),
	_cauchy_gen(_engine, _cauchy_dist) { }

	Random::Random(unsigned int seed)
	:
	_seeder(),
	_seed(seed),
	_engine(seed),
	_integer_dist(),
	_integer_gen(_engine, _integer_dist),
	_real_dist(),
	_real_gen(_engine, _real_dist),
	_normal_dist(),
	_normal_gen(_engine, _normal_dist),
	_cauchy_dist(),
	_cauchy_gen(_engine, _cauchy_dist) { }
    
    Random *Random::getInstance() {
        if(Random::instance == NULL){
            Random::instance = new Random();
            std::cout << BOLDMAGENTA << "Initialized new random instance with seed: " << Random::instance->seed() << RESET << std::endl;
        }
        return Random::instance;
    }
    
    Random::~Random() {
        if(Random::instance){
            delete Random::instance;
            Random::instance = NULL;
        }
    }

	unsigned int Random::seed() const
	{
		return _seed;
	}

	unsigned int Random::uniform_integer()
	{
		return _integer_gen();
	}
    
    int Random::uniform_integer(int min, int max) {
        boost::random::uniform_int_distribution<> dist(min, max-1);
        return dist(_engine);
    }

	double Random::uniform_real()
	{
		return _real_gen();
	}

	double Random::normal_real()
	{
		return _normal_gen();
	}
    
    double Random::uniform_real(int min, int max)
    {
        boost::random::uniform_real_distribution<double> dist(min, max);
        return dist(_engine);
    }

	double Random::cauchy_real()
	{
		return _cauchy_gen();
	}

	unsigned int Random::operator ()()
	{
		return _seeder();
	}
}