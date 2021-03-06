#ifndef GLOBALS_H
#define	GLOBALS_H

#include "tinyxmlplus.h"

#include <boost/noncopyable.hpp>

#include <map>
#include <string>
#include "Logger.h"

namespace POWER
{

	class Globals
	:
	public boost::noncopyable
	{
	  public:

		/**
		 * Class name for XML
		 */
		static const std::string XML_NAME;

		/**
		 * Returns a unique class instance initialized to default values
		 * @return Globals singleton with default values
		 */
		static Globals * init();

		/**
		 * Returns a unique class instance reading values from a file
		 * @param
		 * @return Globals singleton with default values
		 */
		static Globals * load(const std::string &);

		/**
		 * Returns the current class instance
		 * @return Globals singleton instance
		 */
		static Globals * instance();

		/**
		 * Saves the current class instance to a file
		 * @param File path
		 */
		static void save(const std::string &);

		/**
		 * Destroys the unique class instance releasing memory
		 */
		static void deinit();

		/**
		 * Returns the value of the specified parameter
		 * @param Parameter name
		 * @return Parameter value
		 */
		double parameter(const std::string &) const;

		/**
		 * Changes the current seed
		 * @param New seed value
		 */
		//void seed(unsigned int);

		/**
		 * Changes the value of an existing parameter or adds a new parameter
		 * with specified name and value
		 * @param Parameter name
		 * @param Parameter value
		 */
		void parameter(const std::string &, double);

		// </editor-fold>

		/**
		 * Save class internal data in XML format
		 * @return XML Element representing class
		 */
		TiXmlElement save_xml() const;

	  private:

		typedef std::map<std::string, double>::iterator MIterator;
		typedef std::map<std::string, double>::const_iterator MConstIterator;

		Globals();
		Globals(const std::string &);

		virtual ~Globals();

		std::map<std::string, double> _init_parameters();

		static Globals * _instance;

		std::map<std::string, double> _tuples;
        
        log4cpp::Category& logger;

	};
}
#endif	/* GLOBALS_H */