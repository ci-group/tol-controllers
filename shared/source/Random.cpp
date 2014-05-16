#include "Random.h"

namespace utils
{

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

	unsigned int Random::seed() const
	{
		return _seed;
	}

	unsigned int Random::uniform_integer()
	{
		return _integer_gen();
	}

	double Random::uniform_real()
	{
		return _real_gen();
	}

	double Random::normal_real()
	{
		return _normal_gen();
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