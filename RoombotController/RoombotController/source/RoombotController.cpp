#include "RoombotController.h"
#include "MatrixGenomeManager.h"


const std::string RoombotController::GPS_NAME = "GPS";

const std::string RoombotController::BUFFER_NAME = "Buffer";
const std::string RoombotController::RESULTS_DIR = "Results";

const int RoombotController::GPS_LOG_PERIOD = 50;


/********************************************* CONSTRUCTORS *********************************************/

RoombotController::RoombotController():

// read parameters tree
_parameters(_init_parameters(getControllerArguments())),

// set attributes with the parameters' values
_name(_parameters->get<std::string>("Robot.Name")),
_r_index(_parameters->get<std::size_t>("Robot.Index")),
_r_index_root(_parameters->get<std::size_t>("Robot.Index_Root")),
organismSize(_parameters->get<std::size_t>("Robot.Modules_#")),
_m_index(_parameters->get<std::size_t>("Robot." + getName() + ".Index")/* - _r_index*/),
_m_type(_parameters->get<int>("Robot." + getName() + ".Type")),
_ev_type(_parameters->get<int>("Algorithm.Type")),
totalEvaluations(_parameters->get<unsigned int>("Algorithm.Evaluations")),
evaluationDuration (_parameters->get<unsigned int>("Algorithm.Infancy_Duration")),
matureTimeToLive (_parameters->get<unsigned int>("Algorithm.Mature_Time_To_Live")),
_ev_angular_velocity(_parameters->get<double>("Algorithm.Angular_Velocity")),
genome(_parameters->get<std::string>("Genome")),
mindGenome(_parameters->get<std::string>("MindGenome")),

// set other attributes to initial values
fertile(false),
_seed(0),
_time_start(0.0),
_time_offset(0.0),
_time_end(0.0),
_ev_step(0),
_position_start(transforms::Vector_3::ZERO),
_position_end(transforms::Vector_3::ZERO),
numMotors(0),
motorRange(0),
_algorithm(0),
TIME_STEP(getBasicTimeStep()),
_gps(isRoot() ? _init_gps(TIME_STEP) : 0),
_motors(_m_type ? _init_motors(TIME_STEP) : 0)
{
    std::string phase = "constructor";
    try
    {
        simulationDateAndTime = _parameters->get<std::string>("Simulation");
        
        istringstream(_name.substr(_name.find("_") + 1, _name.length())) >> organismId;
        
        std::cout << "[" << getTime() << "] " << getName() << ": " << "Starting Controller, timestep: " << TIME_STEP  << std::endl;
#ifdef DEBUG_CONTROLLER
        std::cout << "[" << getTime() << "] " << getName() << ": "
        << _parameters << " "
        << _name << " "
        << _seed << " "
        << _r_index << " "
        << _r_index_root << " "
        << _r_size << " "
        << _m_index << " "
        << _m_type << " "
        << _time_step << " "
        << _time_start << " "
        << _time_offset << " "
        << _time_end << " "
        << _ev_type << " "
        << _ev_step << " "
        << _ev_steps_recovery << " "
        << _ev_steps_total << " "
        << _position_start << " "
        << _position_end << " "
        << _algorithm << " "
        << _gps << " "
        << _emitter << " "
        << _receiver << " "
        << _motors << " "
        << std::endl;
#endif
        
        if(PARENT_SELECTION == "BESTTWO") {
            parentSelectionMechanism = new BestTwoParentSelection();
        } else if(PARENT_SELECTION == "BINARY_TOURNAMENT"){
            parentSelectionMechanism = new BinaryTournamentParentSelection();
        } else if (PARENT_SELECTION == "RANDOM") {
            parentSelectionMechanism = new RandomSelection();
        } else {
            std::cerr << "Unknown Parent Selection Mechanism: " << std::endl;
        }
        
        if(MATING_SELECTION == "EVOLVER") {
            matingType = MATING_SELECTION_BY_EVOLVER;
        } else if(MATING_SELECTION == "ORGANISMS"){
            matingType = MATING_SELECTION_BY_ORGANISMS;
        } else {
            std::cerr << "Unknown Mating Selection Mechanism: " << MATING_SELECTION << std::endl;
        }
        
        if(DEATH_SELECTION == "EVOLVER") {
            deathType = DEATH_SELECTION_BY_EVOLVER;
        } else if(DEATH_SELECTION == "TIME_TO_LIVE"){
            deathType = DEATH_SELECTION_BY_TIME_TO_LIVE;
        } else {
            std::cerr << "Unknown Death Selection Mechanism: " << DEATH_SELECTION << std::endl;
        }
        
        deathReceiver = getReceiver(ROOMBOT_DEATH_RECEIVER_NAME);
        deathReceiver->setChannel(DEATH_CHANNEL);
        deathReceiver->enable(TIME_STEP);
        while (deathReceiver->getQueueLength() > 0)
        {
            deathReceiver->nextPacket();
        }
        
        genomeReceiver = getReceiver(ROOMBOT_GENOME_RECEIVER_NAME);
        genomeEmitter = getEmitter(ROOMBOT_GENOME_EMITTER_NAME);
        
        genomeReceiver->disable(); // always disabled for non-root modules, will become enabled for the root when organism becomes fertile
        
        // lock connectors
        boost::property_tree::ptree connectrsNode = _parameters->get_child("Robot." + getName() + ".Connectors");
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, connectrsNode)
        {
            Connector * connector = getConnector(v.second.data());
            connector->lock();
            connectors.push_back(connector);
        }
        
        // initialization for non-root module
        if (!isRoot())
        {
            // set non-root module's emitter and receiver using the intex of the root
            _emitter = _init_emitter(static_cast<int> (organismId));
            _receiver = _init_receiver(TIME_STEP, static_cast<int> (EVOLVER_CHANNEL-1 - organismId));
            
            return;
        }
        
        // initialization for root module
        else
        {
            // set root module's emitter and receiver as inverse of the ones for non-root modules
            // in order to make possible the communitation between master and slave
            _emitter = _init_emitter(static_cast<int> (EVOLVER_CHANNEL-1 - organismId));
            _receiver = _init_receiver(TIME_STEP, static_cast<int> (organismId));
            
            if (matingType == MATING_SELECTION_BY_EVOLVER)
            {
                genomeEmitter->setRange(-1);
                genomeEmitter->setChannel(EVOLVER_CHANNEL);
            }
            else if (matingType == MATING_SELECTION_BY_ORGANISMS)
            {
                genomeEmitter->setRange(ORGANISM_GENOME_EMITTER_RANGE);
                genomeEmitter->setChannel(GENOME_EXCHANGE_CHANNEL);
            }
            genomeReceiver->setChannel(GENOME_EXCHANGE_CHANNEL);    // USED ONLY FOR DISTRIBUTED REPRODUCTION
            
            // set variables for writing on files
            utils::Random rng;
            std::string parametersPath = _parameters->get<std::string>("Algorithm.Parameters");
            std::string save_path = _parameters->get<std::string>("Algorithm.Save");
            
            /*boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%d-%b-%Y %H:%M:%S");
             cout.imbue(locale(cout.getloc(), facet));
             std::stringstream dateStream;
             dateStream << boost::posix_time::second_clock::local_time();
             logDirectory = RESULTS_PATH + dateStream.str() + "/";
             std::cout << "LOG PATH: " << logDirectory << std::endl;*/
            
            logDirectory = _parameters->get<std::string>("Algorithm.LogDir","");
            
            // set attributes for specific algorithm
            
            bool flag = true;   // needed for the generetion of an unused seed
            
            switch (_ev_type)
            {
                    
                case A_NEAT:
                    
                    while (flag) {
                        flag = false;
                        try {
                            _seed = rng.seed();
                            logDirectory = _init_directory(getProjectPath(), _name, "HyperNEAT", logDirectory, _seed);
                        }
                        catch (...) {
                            flag = true;
                        }
                    }
                    
                    if (save_path.empty()) {
                        _algorithm = new HyperNEAT(
                                                   _seed,
                                                   parametersPath,
                                                   logDirectory,
                                                   _init_shape_size(_parameters->get_child("Shape")),
                                                   initialiseModuleMapping(_r_index, organismSize, _name, _parameters->get_child("Robot")),
                                                   numMotors,
                                                   1);
                    }
                    else {
                        //_algorithm = new HyperNEAT(save_path);
                    }
                    
                    break;
                    
                case A_POWER:
                    
                    while (flag) {
                        flag = false;
                        try {
                            _seed = rng.seed();
                            logDirectory = _init_directory(getProjectPath(), _name, "RL_PoWER", logDirectory, _seed);
                        }
                        catch (...) {
                            flag = true;
                        }
                    }
                    
                    if (save_path.empty()) {
                        _algorithm = new RL_PoWER(
                                                  _seed,
                                                  "",
                                                  logDirectory,
                                                  TIME_STEP * 1e-3,    // Webots time step converted from ms to s
                                                  _ev_angular_velocity,
                                                  totalEvaluations,
                                                  organismSize * numMotors
                                                  );
                    }
                    else {
                        _algorithm = new RL_PoWER(
                                                  save_path,
                                                  TIME_STEP * 1e-3, // Webots time step converted from ms to s
                                                  _ev_angular_velocity,
                                                  organismSize * numMotors);
                    }
                    
                    break;
                    
                case A_CPG:
                    
                    throw std::runtime_error("Algorithm Unsupported");
                    break;
                    
                case A_SPLINENEAT:
                    
                    while (flag) {
                        flag = false;
                        try {
                            _seed = rng.seed();
                            logDirectory = _init_directory(getProjectPath(), _name, "SplineNEAT", logDirectory, _seed);
                        }
                        catch (...) {
                            flag = true;
                        }
                    }
                    
                    _algorithm = new SplineNeat(
                                                _seed,
                                                parametersPath,
                                                logDirectory,
                                                initialiseModuleMapping(_r_index, organismSize, _name, _parameters->get_child("Robot")),
                                                numMotors,
                                                TIME_STEP * 1e-3,  // Webots time step converted from ms to s
                                                _ev_angular_velocity,
                                                (unsigned int)organismSize);
                    
                    break;
                    
                default:
                    
                    throw std::runtime_error("Algorithm Unknown");
                    break;
            }
            
            std::cout << "Algorithm: " << &_algorithm << std::endl;
            
        }
    }catch (int err) {
        std::string message = "error number" + std::to_string(err);
        std::cerr << "PROBLEM in " << phase << ": " << message << std::endl;
        storeProblem(message, phase);
    }
    catch (std::exception e) {
        std::string message = e.what();
        std::cerr << "PROBLEM in " << phase << ": " << message << std::endl;
        storeProblem(message, phase);
    }
    catch (...) {
        std::string message = "something else";
        std::cerr << "PROBLEM in " << phase << ": " << message << std::endl;
        storeProblem(message, phase);
    }
}


RoombotController::~RoombotController()
{
    if (_algorithm) {
        delete(_algorithm);
    }
    if (_parameters)
    {
        delete(_parameters);
    }
}

/********************************************* MINOR FUNCTIONS *********************************************/

// read parameters tree
boost::property_tree::ptree * RoombotController::_init_parameters(const std::string & path)
{
    boost::property_tree::ptree * result = new boost::property_tree::ptree();
    boost::property_tree::read_json(path, (* result));
    return result;
}

// emitter initialization
Emitter * RoombotController::_init_emitter(int channel)
{
    Emitter * emitter = getEmitter(ROOMBOT_EMITTER_NAME);
    if (!emitter) {
        throw std::runtime_error("Device Not Found");
    }
    emitter->setChannel(channel);
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Init Emitter" << std::endl;
#endif
    return emitter;
}

// receiver initialization
Receiver * RoombotController::_init_receiver(double time_step, int channel)
{
    Receiver * receiver = getReceiver(ROOMBOT_RECEIVER_NAME);
    if (!receiver) {
        throw std::runtime_error("Device Not Found");
    }
    receiver->enable(time_step);
    receiver->setChannel(channel);
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Init Receiver" << std::endl;
#endif
    return receiver;
}

// get the angles for each motors of each module of the organisms
doubledvector RoombotController::receiveAngles()
{
    doubledvector result(organismSize,dvector(numMotors,0.0));
    
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] receiveAngles " << getName() << ": queue length: " << _receiver->getQueueLength()  << std::endl;
#endif
    while(_receiver->getQueueLength() > 0){
#ifdef DEBUG_CONTROLLER
        std::cout << "[" << getTime() << "] receiveAngles " << getName() << ": received data of size: " << _receiver->getDataSize()
        << " data: " << (char*)_receiver->getData() << std::endl;
#endif
        
        using namespace boost::property_tree;
        ptree data;
        std::istringstream stream((char*)_receiver->getData());
        boost::property_tree::json_parser::read_json(stream, data);
        
        int index = data.get<double>("index");
        for(int j=0;j<numMotors;j++)
        {
            result[index][j] = data.get<double>(std::to_string(j));
        }
        
#ifdef DEBUG_CONTROLLER
        std::cout  std::cout << "[" << getTime() << "] " << getName() << " received vector with: " << result.size() << " elements" << std::endl;
#endif
        
#ifdef DEBUG_TIMING
        std::cout << "[" << getTime() << "] " << getName() << " received angles of timestep: " << data.get<double>("timestamp") << std::endl;
#endif
        
        _receiver->nextPacket();
        
    }
    
    return result;
}

// compute new angles for each motors of each module of the organisms
std::vector<std::vector<double> > RoombotController::_compute_angles(const std::vector<std::vector<double> > & angles)
{
    size_t size = angles.size();
    std::vector<std::vector<double> > result(size,std::vector<double>(numMotors,0.0));
    
    _algorithm->reset();
    
    for (std::size_t index = 0; index < size; index++) {
        for(size_t j=0;j<numMotors;j++){
            _algorithm->setParameter((index*numMotors)+j, angles[index][j]);
        }
    }
    
    _algorithm->setParameter(organismSize*numMotors, std::sin(_ev_angular_velocity * (getTime() - _time_offset)));
    
    _algorithm->update();
    
    for (std::size_t index = 0; index < size; index++) {
        for(size_t j=0;j<numMotors;j++){
            result[index][j] = _algorithm->getParameter((index*numMotors)+j);
        }
    }
    
    return result;
}

// use the emitter to send motors' angles
void RoombotController::sendAngles(doubledvector anglesOut){
    if(anglesOut.size() > 0){
        using namespace boost::property_tree;
        ptree data;
        data.put("timestamp",getTime());
        for(int i=0;i<anglesOut.size();i++){
            ptree sub;
            for(int j=0;j<anglesOut[i].size();j++){
                sub.put(std::to_string(j),anglesOut[i][j]);
            }
            data.add_child(std::to_string(i), sub);
        }
        
        std::ostringstream datastring;
        boost::property_tree::json_parser::write_json(datastring,data,false);
#ifdef DEBUG_CONTROLLER
        std::cout << "[" << getTime() << "] sendAngles " << getName()
        << " sending data: " << datastring.str() << std::endl;
#endif
        _emitter->send(datastring.str().c_str(),(int)datastring.str().size()+1);
    }
}

// compute own fitness
std::pair<double, std::string> RoombotController::_compute_fitness(double d_t, const transforms::Vector_3 & d_xyz)
{
    std::pair<double, std::string> result;
    
    switch (_ev_type) {
        case A_NEAT:
        case A_SPLINENEAT:
            result.first = std::pow(2, d_xyz.lenght_square());
            break;
            
        case A_CPG:
            result.first = d_xyz.lenght();
            break;
            
        case A_POWER:
            result.first = d_xyz.lenght() / d_t;
            break;
            
        default:
            throw std::runtime_error("Unknown Algorithm");
            break;
    }
    
    result.second = boost::lexical_cast<std::string>(std::pow(2, d_xyz.lenght_square()))
    + " " +
    boost::lexical_cast<std::string>(d_xyz.lenght())
    + " " +
    boost::lexical_cast<std::string>(d_xyz.lenght() / d_t)
    + " " +
    boost::lexical_cast<std::string>(std::pow(100 * (d_xyz.lenght() / d_t),6));
    
    return result;
}

// get REAL fitness (for A_POWER)
double RoombotController::getRealFitness(double fitness)
{
    double result;
    switch (_ev_type) {
        case A_NEAT:
        case A_SPLINENEAT:
        case A_CPG:
            result = fitness;
            break;
            
        case A_POWER:
            result = std::pow(100 * fitness,6);
            break;
            
        default:
            throw std::runtime_error("Unknown Algorithm");
            break;
    }
    
    return result;
}

// send angles using emitter
void RoombotController::sendAngles(size_t index, dvector anglesOut){
    if(anglesOut.size() > 0){
        using namespace boost::property_tree;
        ptree data;
        data.put("index",std::to_string(index));
        data.put("timestamp",getTime());
        for(int j=0;j<anglesOut.size();j++){
            data.put(std::to_string(j),anglesOut[j]);
        }
        
        std::ostringstream datastring;
        boost::property_tree::json_parser::write_json(datastring,data,false);
#ifdef DEBUG_CONTROLLER
        std::cout << "[" << getTime() << "] sendAngles(index) " << getName()
        << " sending data: " << datastring.str() << std::endl;
#endif
        _emitter->send(datastring.str().c_str(),(int)datastring.str().size()+1);
    }
}

// receive angles using receiver
dvector RoombotController::receiveAngles(size_t index){
    dvector result(numMotors,0.0);
    
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] receiveAngles(index) " << getName() << ": queue length: " << _receiver->getQueueLength()  << std::endl;
#endif
    if(_receiver->getQueueLength()){
#ifdef DEBUG_CONTROLLER
        std::cout << "[" << getTime() << "] receiveAngles(index) " << getName() << ": received data of size: " << _receiver->getDataSize()
        << " data: " << (char*)_receiver->getData() << std::endl;
#endif
        
        using namespace boost::property_tree;
        std::istringstream stream((char*)_receiver->getData());
        ptree data;
        boost::property_tree::json_parser::read_json(stream, data);
        
        ptree sub = data.get_child(std::to_string(index));
        for(int j=0;j<numMotors;j++){
            result[j] = sub.get<double>(std::to_string(j));
        }
#ifdef DEBUG_CONTROLLER
        std::cout << "Received vector with: " << result.size() << " elements:" << std::endl;
        for(int j=0;j<MOTOR_SIZE;j++){
            std::cout << result[j] << ", ";
        }
        std::cout << std::endl;
#endif
        
#ifdef DEBUG_TIMING
        std::cout << "[" << getTime() << "] " << getName() << " got angles with timestamp: " << data.get<double>("timestamp") << std::endl;
#endif
        _receiver->nextPacket();
    }
#ifdef DEBUG_CONTROLLER
    else{
        std::cerr << "[" << getTime() << "] " << getName() << " did not receive any angle updates from root!" << std::endl;
    }
#endif
    
    return result;
}

// initialize GPS
GPS * RoombotController::_init_gps(double time_step)
{
    GPS * gps = getGPS(RoombotController::GPS_NAME);
    
    if (!gps) {
        throw std::runtime_error("Device Not Found");
    }
    
    gps->enable(time_step);
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Init GPS" << std::endl;
#endif
    return gps;
}

// get GPS position
transforms::Vector_3 RoombotController::_get_gps()
{
    const double * values = _gps->getValues();
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Get GPS Coordinates " << values[0] << " 0.0 " << values[2] << std::endl;
#endif
    return transforms::Vector_3(values[0], 0.0, values[2]);
}

double RoombotController::_get_motor_position(size_t index)
{
    double value = (utils::MyMath::PI_HALF_INVERSE * _motors[index]->getPosition());
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Get Servo Angle " << value << std::endl;
#endif
    return value;
}

// initialize motors
Motor** RoombotController::_init_motors(double time_step)
{
    numMotors = _parameters->get<unsigned int>("Robot.Motor.Number");
    motorRange = _parameters->get<double>("Robot.Motor.Range");
    
    Motor** motor = new Motor*[numMotors];
    for(size_t i=0;i<numMotors;i++){
        std::string motorName("Robot.Motor."+std::to_string(i));
        
        motor[i] = getMotor(_parameters->get<std::string>(motorName));
        if (!motor[i]) {
            throw std::runtime_error("Device Not Found");
        }
        motor[i]->enablePosition(time_step);
    }
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Init Servo" << std::endl;
#endif
    return motor;
}

// set new position of motors
void RoombotController::_set_motor_position(size_t index, double value)
{
    if (value < -1.0) {
        value = -1.0;
    }
    if (value > 1.0) {
        value = 1.0;
    }
    
    value *= motorRange;
    
    _motors[index]->setPosition(value);
    
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Set Servo Angle " << value << std::endl;
#endif
}

// initialize directory
std::string RoombotController::_init_directory(const std::string & directory, const std::string & name, const std::string & algorithm, const std::string &logdir, unsigned int instance)
{
    boost::filesystem::path path(directory);
    
    path /= RoombotController::RESULTS_DIR;
    
    if(simulationDateAndTime == "random"){
        std::time_t rawtime;
        std::tm* timeinfo;
        char buffer [80];
        std::time(&rawtime);
        timeinfo = std::localtime(&rawtime);
        std::strftime(buffer,80,"%Y-%m-%d-%H.%M.%S",timeinfo);
        
        simulationDateAndTime = buffer;
    }
    
    path /= simulationDateAndTime;
    
    path /= name;
    
    path /= algorithm;
    
    if(logdir.length() > 0){
        path /= logdir;
    }
    
    //    path /= boost::lexical_cast<std::string>(instance);
    
    if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path)) {
        boost::filesystem::remove_all(path);
        //throw std::runtime_error("Directory Already Existing");
    }
    
    boost::filesystem::create_directories(path);
    
    return path.string();
}

// initialize shape size
transforms::Vector_3 RoombotController::_init_shape_size(const boost::property_tree::ptree & root)
{
    transforms::Vector_3 temp;
    
    temp.import_from_ptree(root);
    
    return transforms::Vector_3(temp.z(), temp.x(), temp.y());
}

// initialize module mapping
std::vector<transforms::Vector_3> RoombotController::initialiseModuleMapping(std::size_t offset, std::size_t size, const std::string & name, const boost::property_tree::ptree & root)
{
    transforms::Vector_3 p_index;
    boost::property_tree::ptree parent = root, child;
    
    std::vector<transforms::Vector_3> result(size);
    
    for (std::size_t index = 0; index < size; index++) {
        /* Get Module Property Tree */
        child = parent.get_child(name + ":" + boost::lexical_cast<std::string>(index + offset));
        
        /* Get Module 3D Index */
        p_index.import_from_ptree(child.get_child("Index_3D"));
        
        result[index] = transforms::Vector_3(p_index.z(), p_index.x(), p_index.y());
        
        //std::cout << index << " -> " << result[index] << std::endl;
    }
#ifdef DEBUG_CONTROLLER
    std::cout << "[" << getTime() << "] " << getName() << ": Init Servo Mapping" << std::endl;
#endif
    return result;
}

// loggin for GPS
void RoombotController::logGPS(){
    const double * values = _gps->getValues();
    gpsLog << _algorithm->getEvaluation() << " " << values[0] << " 0.0 " << values[2] << std::endl;
}

// check if a module is root
bool RoombotController::isRoot() {
    return _m_type == M_ROOT;
}


void RoombotController::storeMatureLifeFitnessIntoFile(double fitness)
{
    ofstream fitnessFile;
    fitnessFile.open(RESULTS_PATH + simulationDateAndTime + "/organism_" + std::to_string(organismId) + "/mature_life_fitness.txt", ios::app);
    fitnessFile << getTime() << " " << fitness << std::endl;
    fitnessFile.close();
}


void RoombotController::storeFertilityOnFile()
{
    ofstream fitnessFile;
    fitnessFile.open(RESULTS_PATH + simulationDateAndTime + "/fertility.txt", ios::app);
    fitnessFile << getTime() << " " << organismId << std::endl;
    fitnessFile.close();
}


void RoombotController::storeRebuild(std::string message)
{
    ofstream rebuildFile;
    rebuildFile.open(RESULTS_PATH + simulationDateAndTime + "/rebuild.txt", ios::app);
    rebuildFile << getTime() << " " << organismId << " " << message << std::endl;
    rebuildFile.close();
}


void RoombotController::storeProblem(std::string message, std::string phase)
{
    ofstream problemFile;
    problemFile.open(RESULTS_PATH + simulationDateAndTime + "/problems.txt", ios::app);
    problemFile << getTime() << " " << getName() << " PROBLEM IN " << phase << ": " << message << std::endl;
    problemFile.close();
}


bool RoombotController::allConnectorsOK()
{
    for (int i = 0; i < connectors.size(); i++)
    {
        if (connectors[i]->getPresence() != 1)
        {
            return false;
        }
    }
    return true;
}


void RoombotController::enableAllConnectors()
{
    for (int i = 0; i < connectors.size(); i++)
    {
        connectors[i]->enablePresence(TIME_STEP/4);
    }
}


void RoombotController::disableAllConnectors()
{
    for (int i = 0; i < connectors.size(); i++)
    {
        connectors[i]->disablePresence();
    }
}


void RoombotController::readMateMessage(std::string message, id_t * mateId, double * mateFitness, std::string * mateGenome, std::string * mateMind)
{
    * mateId = std::atoi(MessagesManager::get(message, "ID").c_str());
    * mateFitness = std::atof(MessagesManager::get(message, "FITNESS").c_str());
    * mateGenome = MessagesManager::get(message, "GENOME");
    * mateMind = MessagesManager::get(message, "MIND");
}


void RoombotController::updateOrganismsToMateWithList(id_t mateId, double mateFitness, std::string mateGenome, std::string mateMind)
{
    for (int i = 0; i < organismsToMateWith.size(); i++)
    {
        if (organismsToMateWith[i].getId() == mateId)
        {
            organismsToMateWith[i].setFitness(mateFitness);
            return;
        }
    }
    Organism newMate = Organism(mateGenome, mateMind, mateId, mateFitness, 0, 0, std::vector<id_t>(), Organism::ADULT);
    organismsToMateWith.push_back(newMate);
}


id_t RoombotController::selectMate()
{
    std::cout << "SELECT MATE - start" << std::endl;
    std::vector<id_t> selected = parentSelectionMechanism->selectParents(organismsToMateWith);
    std::cout << "SELECT MATE - selected" << std::endl;
    if (selected.size() > 0)
    {
        std::cout << "SELECT MATE - return first" << std::endl;
        return selected[0];
    }
    std::cout << "SELECT MATE - return 0" << std::endl;
    return 0;
}


int RoombotController::searchForOrganism(id_t organismId)
{
    for(int i = 0; i < organismsToMateWith.size(); i++)
    {
        if (organismsToMateWith[i].getId() == organismId)
        {
            return i;
        }
    }
    return -1;
}


void RoombotController::sendAdultAnnouncement()
{
    int backupChannel = _emitter->getChannel();
    _emitter->setChannel(EVOLVER_CHANNEL);
    std::string message = "[ADULT_ANNOUNCEMENT]";
    message = MessagesManager::add(message, "ID", std::to_string(organismId));
    _emitter->send(message.c_str(), (int)message.length()+1);
    _emitter->setChannel(backupChannel);
}

void RoombotController::sendFitnessUpdateToEvolver(double fitness)
{
    int backupChannel = _emitter->getChannel();
    _emitter->setChannel(EVOLVER_CHANNEL);
    std::string message = "[FITNESS_UPDATE]";
    message = MessagesManager::add(message, "ID", std::to_string(organismId));
    message = MessagesManager::add(message, "FITNESS", std::to_string(fitness));
    _emitter->send(message.c_str(), (int)message.length()+1);
    _emitter->setChannel(backupChannel);
}


/******************************************* BEFORE STARTING *******************************************/

bool RoombotController::checkLocks()
{
    enableAllConnectors();
    
    double startingTime = getTime();
    bool connectorsOK = true;
    while (getTime() - startingTime < 2)
    {
        if (step(TIME_STEP) != -1)
        {
            if (!allConnectorsOK())
            {
                connectorsOK = false;
            }
        }
    }
    
    disableAllConnectors();
    
    if (!connectorsOK)
    {
        std::cout << getName() << " detected connectors problem" << std::endl;
        
        std::string message = "[CONNECTORS_PROBLEM_MESSAGE]";
        _emitter->send(message.c_str(), (int)message.length()+1);
        return false;
    }
    
    startingTime = getTime();
    while (getTime() - startingTime < 1)
    {
        if(step(TIME_STEP) == -1) {
            return connectorsOK;
        }
        
        while (_receiver->getQueueLength() > 0)
        {
            std::string message = (char*)_receiver->getData();
            
            if (message.compare("[CONNECTORS_PROBLEM_MESSAGE]") == 0)
            {
                std::cout << getName() << " received connectors problem message" << std::endl;
                
                if (isRoot())
                {
                    std::cout << getName() << " forwarded connectors problem" << std::endl;
                    
                    std::string message = "[CONNECTORS_PROBLEM_MESSAGE]";
                    _emitter->send(message.c_str(), (int)message.length()+1);
                }
                return false;
            }
            
            _receiver->nextPacket();
        }
    }
    
    return connectorsOK;
}


bool RoombotController::checkFallenInside()
{
    bool positionOK = true;
    
    if (isRoot())
    {
        const double * position = _gps->getValues();
        double x = position[0];
        double z = position[2];
        double distanceFromCenter = sqrt((x*x) + (z*z));
        
        if (distanceFromCenter < 2)
        {
            std::cout << getName() << " detected fallen into cylinder problem" << std::endl;
            
            std::string message = "[CYLINDER_PROBLEM_MESSAGE]";
            _emitter->send(message.c_str(), (int)message.length()+1);
            return false;
        }
    }
    else
    {
        double startingTime = getTime();
        while (getTime() - startingTime < 1)
        {
            if(step(TIME_STEP) == -1) {
                return positionOK;
            }
            
            while(_receiver->getQueueLength() > 0)
            {
                std::string message = (char*)_receiver->getData();
                
                if (message.compare("[CYLINDER_PROBLEM_MESSAGE]") == 0)
                {
                    std::cout << getName() << " received fallen into cylinder problem" << std::endl;
                    
                    return false;
                }
                
                _receiver->nextPacket();
            }
        }
    }
    
    return positionOK;
}



/******************************************* INFANCY *******************************************/

// initial learning phase of the organism's life
void RoombotController::infancy()
{
    // ROOT MODULES' BEHAVIOR
    
    if (isRoot()) {
        
#ifdef DEBUG_CONTROLLER
        std::cout << "[" << getTime() << "] " << getName() << ": We are root initialize things" << std::endl;
#endif
        
        // set variables for writing on files
        boost::filesystem::path dirpath(logDirectory);
        std::cout << "Opening fitness log file: " << dirpath / "fitness.log" << std::endl;
        std::ofstream fitnessLog((dirpath / "fitness.log").c_str(), std::ofstream::app);
        std::cout << "Opening location log file: " << dirpath / "fitness.log" << std::endl;
        gpsLog = std::ofstream((dirpath / "location.log").c_str(), std::ofstream::app);
        fitnessLog << "#Generation Evaluation Fitness 2eDisSq Distance Speed RLFitness  (Seed: " << _seed << ")" << std::endl;
        gpsLog << "#Evaluation x y z" << std::endl;
        
        std::pair<double, std::string> fitness(0.0, ""); // initialize fitness
        
        // initialize motors' angles (notice that root modules are 1 timestep further than the others)
        doubledvector anglesIn = doubledvector(organismSize,std::vector<double>(numMotors,0.0));     // input angles
        doubledvector anglesOut = doubledvector(organismSize,std::vector<double>(numMotors,0.0));    // output angles
        dvector anglesTMinusOne = dvector(numMotors,0.0);   // angles for root at time t-1
        dvector anglesTPlusOne = dvector(numMotors,0.0);    // angles for root at time t+1
        
        // initialize motors position
        for(size_t i=0;i<numMotors;i++)
        {
            _set_motor_position(i,0.0);
        }
        
        double singleEvaluationTime = evaluationDuration / totalEvaluations;
        double timeStep = TIME_STEP / 1000.0;
        int totalEvaluationSteps = singleEvaluationTime / timeStep;
        _ev_steps_recovery = 0;// totalEvaluationSteps / 10;
        _ev_steps_total_infancy = totalEvaluationSteps - _ev_steps_recovery;
        
        
        boost::ptr_vector<MindGenome> genomes;
        MatrixGenomeManager manager;
        if(mindGenome.length() == 0){
            genomes = _algorithm->getRandomInitialMinds();
            mindGenome = manager.genomeArrayToString(genomes);
        }else{
            genomes = manager.readStringToArray(mindGenome);
        }
        
        _algorithm->setInitialMinds(genomes,numMotors,organismSize);
        
        // two first steps (one more than non-root modules)
        if (step(TIME_STEP) == -1)
            return;
        if (step(TIME_STEP) == -1)
            return;
        
        // main cycle for root
        
        bool flag = false;
        _time_offset = getTime();   // remember offset for time calculations
        double tMinusOne,tPlusOne;  // times T-1 and T+1
        
        
        double startingTime = getTime();
        
        while (step(TIME_STEP) != -1 && (getTime() - startingTime < evaluationDuration))
        {
            anglesIn = receiveAngles();     // read current angles
            
#ifdef DEBUG_TIMING
            std::cout << "[" << getTime() << "] " << getName() << " using root angles of timestep: " << tMinusOne << std::endl;
#endif
            // modify root angles with the ones at time T-1
            for(size_t i=0;i<numMotors;i++)
            {
                anglesIn[_m_index][i] = anglesTMinusOne[i];     // notice that _m_index is for sure the root here
                anglesTMinusOne[i] = _get_motor_position(i);    // update anglesTMinusOne for the next iteration
                tMinusOne = getTime();                          // update tMinusOne for the next iteration
            }
            
            anglesOut = _compute_angles(anglesIn);  // compute new angles
            
            sendAngles(anglesOut);                  // send them usign the emitter
            
#ifdef DEBUG_TIMING
            std::cout << "[" << getTime() << "] " << getName() << " setting root angles of time: " << tPlusOne << std::endl;
#endif
            // update real motors position
            for(size_t i=0;i<numMotors;i++)
            {
                _set_motor_position(i,anglesTPlusOne[i]);   // root is one timestep further
                anglesTPlusOne[i] = anglesOut[_m_index][i]; // update anglesTPlusOne for the next iteration
                tPlusOne = getTime();                       // update tPlusOne for the next iteration
            }
            
            // if it is not time for evaluation yet
            if (_ev_step < _ev_steps_total_infancy)
            {
                _ev_step += 1;  // increase current
                
                // if the recovery time ends
                if ((_ev_step - 1) == _ev_steps_recovery) {
                    _time_start = getTime();        // save initial time
                    _position_start = _get_gps();   // save initial position
                }
                
                // GPS logging
                if(_ev_step % GPS_LOG_PERIOD == 0){
                    logGPS();
                }
            }
            
            // if it is time for evaluation
            else
            {
                //                std::cout << "Organism " << organismId << " evaluation " << _algorithm->getGeneration() << ":  ";
                
                _time_end = getTime();      // get final time
                _position_end = _get_gps(); // get final position
                
                fitness = _compute_fitness((_time_end - _time_start), (_position_end - _position_start));   // compute fitness
                _algorithm->setEvaluationFitness(fitness.first);
                _algorithm->setEvaluationFitnessAlt(fitness.second);
                
                fitnessLog << _algorithm->getGeneration() << " " << _algorithm->getEvaluation() << " " << getRealFitness(fitness.first) << " " << fitness.second << std::endl;
                
                if ((!flag) && (!_algorithm->nextEvaluation())) {
                    flag = true;
                    _algorithm->save();
                }
                
                _ev_step = 0;
            }
        }
        _algorithm->save();
    }
    
    // NON-ROOT MODULES BEHAVIOR
    
    else {
        
        // intialize angles
        dvector anglesIn(numMotors,0.0);    // input angles
        dvector anglesOut(numMotors,0.0);   // output angles
        
        // initialize motors
        for(size_t i=0;i<numMotors;i++){
            _set_motor_position(i,0.0);
        }
        
        // one first step
        if (step(TIME_STEP) == -1)
            return;
        
        // main cycle for non-root
        
        double startingTime = getTime();
        
        while (step(TIME_STEP) != -1 && (getTime() - startingTime < evaluationDuration))
        {
            // read angles for each motor
            for(size_t i=0;i<numMotors;i++){
                anglesOut[i] = _get_motor_position(i);
            }
            
            sendAngles(_m_index,anglesOut);         // send angles using emitter
            anglesIn = receiveAngles(_m_index);     // read angles using reveiver
#ifdef DEBUG_CONTROLLER
            std::cout << "[" << getTime() << "] " << getName() << " Received vector with: " << anglesIn.size() << " elements:" << std::endl;
            for(int j=0;j<MOTOR_SIZE;j++){
                std::cout << anglesIn[j] << ", ";
            }
            std::cout << std::endl;
#endif
            // update real motors position
            for(size_t i=0;i<numMotors;i++){
                _set_motor_position(i,anglesIn[i]);
            }
        }
    }
}



/***************************************** MATURE LIFE *****************************************/

void RoombotController::matureLife()
{
    double startingTimeMatureLife = getTime();
    
    // ROOT MODULES BEHAVIOR
    
    if (isRoot())
    {
        std::pair<double, std::string> fitness = std::pair<double, std::string>();
        fitness.first = 0;
        fitness.second = "";
        double lastFitnessSent = 0;
        double lastFitnessUpdate = 0;
        double lastMating = 0;
        double lastEvolverUpdate = 0;
        
        doubledvector anglesIn;
        doubledvector anglesOut;
        dvector anglesTMinusOne;
        dvector anglesTPlusOne;
        
        anglesIn = doubledvector(organismSize,std::vector<double>(numMotors,0.0));
        anglesOut = doubledvector(organismSize,std::vector<double>(numMotors,0.0));
        
        anglesTMinusOne = dvector(numMotors,0.0);
        anglesTPlusOne = dvector(numMotors,0.0);
        
        for(size_t i=0;i<numMotors;i++)
        {
            _set_motor_position(i,0.0);
        }
        
        _time_offset = getTime();
        double tMinusOne,tPlusOne;
        
        //_ev_step = 0; // reset evaluation step
        
        _time_start = getTime();        // save initial time
        _position_start = _get_gps();
        
        while (step(TIME_STEP) != -1)
        {
            double now = getTime();
            
            /***************************************************************
             ****** CHECK FERTILITY FOR MATING SELECTION BY ORGANISMS ******
             ***************************************************************/
            
            if (matingType == MATING_SELECTION_BY_ORGANISMS)
            {
                if (!fertile)
                {
                    const double * position = _gps->getValues();
                    double x = position[0];
                    double z = position[2];
                    double distanceFromCenter = sqrt((x*x) + (z*z));
                    
                    if (distanceFromCenter > FERTILITY_DISTANCE)
                    {
                        fertile = true;
                        
                        genomeReceiver->enable(TIME_STEP);
                        while (genomeReceiver->getQueueLength() > 0)
                        {
                            genomeReceiver->nextPacket();
                        }
                        
                        storeFertilityOnFile();
                        std::cout << organismId << " became fertile" << std::endl;
                    }
                }
            }
            
            
            /********************************************
             ***** CHECK DEATH BY EVOLVER SELECTION *****
             ********************************************/
            
            if (deathType == DEATH_SELECTION_BY_EVOLVER)
            {
                while(deathReceiver->getQueueLength() > 0)
                {
                    std::string message = (char*)deathReceiver->getData();
                    deathReceiver->nextPacket();
                    if (message.compare(std::to_string(organismId)) == 0 )
                    {
                        return; // end mature life (so die)
                    }
                }
            }
            
            
            /***************************************
             ***** CHECK DEATH BY TIME TO LIVE *****
             ***************************************/
            
            if (deathType == DEATH_SELECTION_BY_TIME_TO_LIVE)
            {
                if (now - startingTimeMatureLife > matureTimeToLive)
                {
                    return; // end mature life (so die)
                }
            }
            
            
            /**************************
             ***** UPDATE FITNESS *****
             **************************/
            
            if (now - lastFitnessUpdate > UPDATE_FITNESS_INTERVAL)
            {
                // compute fitness
                _time_end = getTime();
                _position_end = _get_gps();
                fitness = _compute_fitness((_time_end - _time_start), (_position_end - _position_start));
                
                // reset time, position and lastFitnessSent for next fitness evaluation
                _time_start = getTime();
                _position_start = _get_gps();
                lastFitnessUpdate = getTime();
            }
            
            
            /*************************************
             ***** UPDATE FITNESS IN EVOLVER *****
             *************************************/
            
            if (matingType == MATING_SELECTION_BY_ORGANISMS)
            {
                if (now - lastEvolverUpdate > UPDATE_FITNESS_IN_EVOLVER)
                {
                    sendFitnessUpdateToEvolver(fitness.first);
                    lastEvolverUpdate = getTime();
                }
            }
            
            
            /*******************
             *** STEP MOTORS ***
             *******************/
            
            anglesIn = receiveAngles();
            
            for(size_t i=0;i<numMotors;i++)
            {
                anglesIn[_m_index][i] = anglesTMinusOne[i];
                anglesTMinusOne[i] = _get_motor_position(i);
                tMinusOne = getTime();
            }
            
            anglesOut = _compute_angles(anglesIn);
            
            sendAngles(anglesOut);
            
            for(size_t i=0;i<numMotors;i++)
            {
                _set_motor_position(i,anglesTPlusOne[i]);
                anglesTPlusOne[i] = anglesOut[_m_index][i];
                tPlusOne = getTime();
            }
            
            
            /**********************************
             ***** SEND GENOME TO EVOLVER *****
             **********************************/
            
            if (matingType == MATING_SELECTION_BY_EVOLVER)
            {
                if (now - lastFitnessSent > SEND_FITNESS_TO_EVOLVER_INTERVAL)
                {
                    // send genome and fitness to evolver
                    string message = "[GENOME_SPREAD_MESSAGE]";
                    message = MessagesManager::add(message, "ID", std::to_string(organismId));
                    message = MessagesManager::add(message, "FITNESS", std::to_string(fitness.first));
                    message = MessagesManager::add(message, "GENOME", genome);
                    message = MessagesManager::add(message, "MIND", mindGenome);
                    genomeEmitter->send(message.c_str(), (int)message.length()+1);
                    
                    // store in file
                    storeMatureLifeFitnessIntoFile(fitness.first);
                    
                    lastFitnessSent = getTime();
                }
            }
            
            
            /***********************************************
             ***** MATING USING SELECTION BY ORGANISMS *****
             ***********************************************/
            
            if (matingType == MATING_SELECTION_BY_ORGANISMS)
            {
                if (fertile)
                {
                    // sending
                    
                    if (now - lastFitnessSent > SPREAD_FITNESS_INTERVAL)
                    {
                        // spread genome and fitness
                        std::string genomeMessage = "[GENOME_SPREAD_MESSAGE]";
                        genomeMessage = MessagesManager::add(genomeMessage, "ID", std::to_string(organismId));
                        genomeMessage = MessagesManager::add(genomeMessage, "FITNESS", std::to_string(fitness.first));
                        genomeMessage = MessagesManager::add(genomeMessage, "GENOME", genome);
                        genomeMessage = MessagesManager::add(genomeMessage, "MIND", mindGenome);
                        
                        genomeEmitter->send(genomeMessage.c_str(), (int)genomeMessage.length()+1);
                        
                        lastFitnessSent = getTime();
                    }
                    
                    // receiving
                    
                    while (genomeReceiver->getQueueLength() > 0)
                    {
                        std::string message = (char*)genomeReceiver->getData();
                        
                        if (message.substr(0,23).compare("[GENOME_SPREAD_MESSAGE]") == 0)
                        {
                            id_t mateId;
                            double mateFitness;
                            std::string mateGenome;
                            std::string mateMind;
                            readMateMessage(message, & mateId, & mateFitness, & mateGenome, & mateMind);
                            
                            updateOrganismsToMateWithList(mateId, mateFitness, mateGenome, mateMind);
                        }
                        
                        genomeReceiver->nextPacket();
                    }
                    
                    // mate
                    
                    if (now - lastMating > INDIVIDUAL_MATING_TIME)
                    {
                        if (organismsToMateWith.size() > 0)
                        {
                            id_t mateId = selectMate();
                            int mateIndex = searchForOrganism(mateId);
                            
                            if (mateIndex >= 0)
                            {
                                // send couple of genomes to evolver
                                int backup_channel = _emitter->getChannel();
                                _emitter->setChannel(EVOLVER_CHANNEL);
                                
                                std::string message = "[COUPLE_MESSAGE]";
                                message = MessagesManager::add(message, "ID1", std::to_string(organismId));
                                message = MessagesManager::add(message, "FITNESS1", std::to_string(fitness.first));
                                message = MessagesManager::add(message, "GENOME1", genome);
                                message = MessagesManager::add(message, "MIND1", mindGenome);
                                message = MessagesManager::add(message, "ID2", std::to_string(mateId));
                                message = MessagesManager::add(message, "FITNESS2", std::to_string(organismsToMateWith[mateIndex].getFitness()));
                                message = MessagesManager::add(message, "GENOME2", organismsToMateWith[mateIndex].getGenome());
                                message = MessagesManager::add(message, "MIND2", organismsToMateWith[mateIndex].getMind());
                                
                                _emitter->send(message.c_str(), (int)message.length()+1);
                                _emitter->setChannel(backup_channel);
                                
                                std::cout << organismId << " chose to mate with " << mateId << std::endl;
                            }
                            else
                            {
                                ofstream file;
                                file.open(RESULTS_PATH + simulationDateAndTime + "/roombot_list_problems.txt", ios::app);
                                file << "TIME: " << getTime() << std::endl;
                                file << "MATE: " << mateId << std::endl;
                                file << "LIST:\n" << std::endl;
                                for (int i = 0; i < organismsToMateWith.size(); i++)
                                {
                                    file << " " << organismsToMateWith[i].getId();
                                }
                                file << std::endl;
                                file.close();
                            }
                        }
                        
                        organismsToMateWith = std::vector<Organism>();
                        
                        lastMating = getTime();
                    }
                }
            }
            
        }
    }
    
    // NON-ROOT MODULES BEHAVIOR
    
    else
    {
        /*if (step(_time_step) == -1)
         {
         return;
         }*/
        
        dvector anglesIn(numMotors,0.0);
        dvector anglesOut(numMotors,0.0);
        
        for(size_t i=0;i<numMotors;i++)
        {
            _set_motor_position(i,0.0);
        }
        
        while (step(TIME_STEP) != -1)
        {
            
            if (deathType == DEATH_SELECTION_BY_EVOLVER)
            {
                while (deathReceiver->getQueueLength() > 0)
                {
                    std::string message = (char*)deathReceiver->getData();
                    deathReceiver->nextPacket();
                    if (message.compare(std::to_string(organismId)) == 0 )
                    {
                        return; // end mature life (so die)
                    }
                }
            }
            
            else if (deathType == DEATH_SELECTION_BY_TIME_TO_LIVE)
            {
                if (getTime() - startingTimeMatureLife > matureTimeToLive)
                {
                    return; // and die
                }
            }
            
            
            for(size_t i=0;i<numMotors;i++)
            {
                anglesOut[i] = _get_motor_position(i);
            }
            
            sendAngles(_m_index,anglesOut);
            anglesIn = receiveAngles(_m_index);
            
            for(size_t i=0;i<numMotors;i++)
            {
                _set_motor_position(i,anglesIn[i]);
            }
        }
    }
    
}

/********************************************* DEATH *********************************************/

void RoombotController::death()
{
    try
    {
        if(isRoot())
        {
            genomeReceiver->disable();
        }
        
        for(int i = 0; i < connectors.size(); i++)
        {
            connectors[i]->unlock();
        }
        
        double startingTime = getTime();
        while (getTime() - startingTime < 3)
        {
            for(int i = 0; i < numMotors; i++)
            {
                _set_motor_position(i, 0);
            }
            
            if (step(TIME_STEP) == -1)
            {
                return;
            }
        }
        
        _emitter->setChannel(EVOLVER_CHANNEL);
        std::string message = "[DEATH_ANNOUNCEMENT_MESSAGE]";
        message = MessagesManager::add(message, "ID", std::to_string(organismId));
        _emitter->send(message.c_str(), (int)message.length()+1);
        
        _emitter->setChannel(MODIFIER_CHANNEL);
        message = "[TO_RESERVE_MESSAGE]";
        message = MessagesManager::add(message, "NAME", getName());
        _emitter->send(message.c_str(), (int)message.length()+1);
        
        while (true)
        {
            for(int i = 0; i < numMotors; i++)
            {
                _set_motor_position(i, 0);
            }
            if (step(TIME_STEP) == -1)
            {
                return;
            }
        }
    }
    catch (int err) {
        std::string message = "error number" + std::to_string(err);
        std::cerr << "DEATH PROBLEM: " << message << std::endl;
        storeProblem(message, "DEATH");
        death();
        return;
    }
    catch (std::exception e) {
        std::string message = e.what();
        std::cerr << "DEATH PROBLEM: " << message << std::endl;
        storeProblem(message, "DEATH");
        death();
        return;
    }
    catch (...) {
        std::string message = "something else";
        std::cerr << "DEATH PROBLEM: " << message << std::endl;
        storeProblem(message, "DEATH");
        death();
        return;
    }
}


void RoombotController::askToBeBuiltAgain()
{
    _emitter->setChannel(CLINIC_CHANNEL);
    
    std::string message = "[REBUILD_MESSAGE]";
    message = MessagesManager::add(message, "ID", std::to_string(organismId));
    message = MessagesManager::add(message, "GENOME", genome);
    message = MessagesManager::add(message, "MIND", mindGenome);
    
    _emitter->send(message.c_str(), (int)message.length()+1);
}


/********************************************* RUN *********************************************/

void RoombotController::run()
{
    std::string phase = "start";
    try
    {
        if (step(TIME_STEP) == -1)
        {
            return;
        }
        
        phase = "removing packets from receivers";
        
        while (_receiver->getQueueLength() > 0)
        {
            _receiver->nextPacket();
        }
        while (deathReceiver->getQueueLength() > 0)
        {
            deathReceiver->nextPacket();
        }
        
        // check if all modules are correctly locked to each other
        
        phase = "checking locks";
        
        if (!checkLocks())
        {
            if (isRoot())
            {
                askToBeBuiltAgain();
                storeRebuild("connection problem");
            }
            death();
            return;
        }
        
        phase = "waiting";
        
        // wait for module to not move after sliding down from clinic
        double startingTime = getTime();
        while (getTime() - startingTime < ROOMBOT_WAITING_TIME)
        {
            if (step(TIME_STEP) == -1)
            {
                return;
            }
        }
        
        phase = "checking inside cylinder";
        
        // check if it fell inside the cylinder
        if (checkInsideCylinder && !checkFallenInside())
        {
            if (isRoot())
            {
                askToBeBuiltAgain();
                storeRebuild("fallen into cylinder");
            }
            death();
            return;
        }
        
        std::cout << getName() << " STARTS INFANCY" << std::endl;
        
        phase = "INFANCY";
        
        infancy();
        
        std::cout << getName() << " STARTS MATURE LIFE" << std::endl;
        
        phase = "MATURE LIFE";
        
        if (isRoot())
        {
            sendAdultAnnouncement();
        }
        
        matureLife();
    }
    catch (int err) {
        std::string message = "error number" + std::to_string(err);
        std::cerr << "PROBLEM in " << phase << ": " << message << std::endl;
        storeProblem(message, phase);
    }
    catch (std::exception e) {
        std::string message = e.what();
        std::cerr << "PROBLEM in " << phase << ": " << message << std::endl;
        storeProblem(message, phase);
    }
    catch (...) {
        std::string message = "something else";
        std::cerr << "PROBLEM in " << phase << ": " << message << std::endl;
        storeProblem(message, phase);
    }
    
    std::cout << getName() << " STARTS DEATH" << std::endl;
    
    death();
}