/* -*- coding: utf-8 -*- */

/** 
 * XP with POMDP and Reservoir Computing
 *
 * - load POMDP and generate Trajectories of S,O,A,S',O',R
 * - store Trajectories
 * - load Trajectories 
 * - generer un esn pour apprendre (bonne dimensions
 * - load un esn
 * TODO : try to learn O,A -> S (??????)
 */

#include <iostream>                // std::cout
#include <fstream>                 // std::ofstream
#include <string>                  // std::string
#include <sstream>                 // std::stringdtream
#include <rapidjson/document.h>    // rapidjson's DOM-style API
#include <json_wrapper.hpp>        // JSON::IStreamWrapper

#include <noise.hpp>

#include <pomdp/pomdp.hpp>
#include <pomdp/trajectory.hpp>

#include <reservoir.hpp>
#include <layer.hpp>
#include <ridge_regression.hpp>

#include <gsl/gsl_rng.h>             // gsl random generator
#include <ctime>                     // std::time std::clock
#include <chrono>                    // std::chrono::steady_clock
#include <random>                    // std::uniform_int...
#include <algorithm>                 // fill, std::max_element
#include <pomdp/prog_dynamique.hpp>  // compute_Q

// Parsing command line options
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <utils.hpp>                  // various str_xxx
using namespace utils::rj;
// ********************************************************************* param
std::string*           _filename_pomdp = nullptr;
std::string*           _fileload_traj = nullptr;
std::string*           _filegene_traj = nullptr;
std::string*           _fileload_esn = nullptr;
std::string*           _filegene_esn = nullptr;
std::string*           _fileload_noise = nullptr;
std::string*           _filegene_noise = nullptr;
std::string*           _filegene_output = nullptr;
std::string*           _filegene_learn = nullptr;

WNoise::Data           _wnoise;
unsigned int           _noise_length;
double                 _noise_level;

Model::POMDP*          _pomdp = nullptr;
unsigned int           _length;
Trajectory::POMDP::Data _traj_data;
Trajectory::POMDP::Data _learn_data;

unsigned int           _test_length;
Trajectory::POMDP::Data _test_data;

Reservoir*             _res = nullptr;
Layer*                 _lay = nullptr;
RidgeRegression::Data  _data;
Reservoir::Toutput_size _res_size;
double                  _res_scaling;
double                  _res_radius;
double                  _res_leak;
double                  _regul;
bool                    _verb;

// Fonction valeur
Algorithms::TVal _vQ;

gsl_rng*               _rnd = gsl_rng_alloc( gsl_rng_taus );
// ****************************************************************** free_mem
void free_mem()
{
  if( _pomdp ) delete _pomdp;
  if( _filename_pomdp ) delete _filename_pomdp;
  if( _fileload_traj ) delete _fileload_traj;
  if( _filegene_traj ) delete _filegene_traj;
  if( _fileload_esn ) delete _fileload_esn;
  if( _filegene_esn ) delete _filegene_esn;
  if( _fileload_noise ) delete _fileload_noise;
  if( _filegene_noise ) delete _filegene_noise;
  if( _filegene_output ) delete _filegene_output;
  if( _filegene_learn ) delete _filegene_learn;
}
void free_esn()
{
  if( _res ) delete _res;
  if( _lay ) delete _lay;
}
// ******************************************************************* options
void setup_options(int argc, char **argv)
{
  po::options_description desc("Options");
  desc.add_options()
    ("help,h", "produce help message")
    ("load_pomdp,p", po::value<std::string>(), "load POMDP from JSON file")
    ("gene_traj", po::value<std::string>(), "gene Trajectory into file")
    ("traj_length", po::value<unsigned int>(&_length)->default_value(10), "generate Traj of length ")
    ("gene_esn", po::value<std::string>(), "gene ESN into file")
    ("res_size", po::value<unsigned int>(&_res_size)->default_value(10), "reservoir size")
    ("res_scaling", po::value<double>(&_res_scaling)->default_value(1.0), "reservoir input scaling")
    ("res_radius", po::value<double>(&_res_radius)->default_value(0.99), "reservoir spectral radius")
    ("res_leak", po::value<double>(&_res_leak)->default_value(0.1), "reservoir leaking rate")
    ("gene_noise", po::value<std::string>(), "gene WNoise into file")
    ("length_noise", po::value<unsigned int>(&_noise_length)->default_value(100), "Length of noise to generate")
    ("level_noise",  po::value<double>(&_noise_level)->default_value(0.1), "Level of noise to generate")
    ("gene_samples",  po::value<std::string>(), "Output file for RidgeReg samples")
    ("regul", po::value<double>(&_regul)->default_value(1.0), "regul for RidgeRegrression")
    ("load_traj,t", po::value<std::string>(), "load Trajectory from file")
    ("load_esn,e",  po::value<std::string>(), "load ESN from file")
    ("load_noise,n", po::value<std::string>(), "load WNoise from file")
    ("test_length,l", po::value<unsigned int>(&_test_length)->default_value(10), "Length of test")
    ("output,o",  po::value<std::string>(), "Output file for results")
    ("verb,v", po::value<bool>(&_verb)->default_value(false), "verbose" );
    ;

  // Options en ligne de commande
  po::options_description cmdline_options;
  cmdline_options.add(desc);

  // Options qui sont 'apres'
  po::positional_options_description pod;
  //pod.add( "data_file", 1);

  // Parse
  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).
	      options(desc).positional(pod).run(), vm);

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << desc << std::endl;
      exit(1);
    }
    
    po::notify(vm);
  }
  catch(po::error& e)  { 
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl; 
    std::cerr << desc << std::endl; 
    exit(2);
  } 
  

  // file names
  if (vm.count("load_pomdp")) {
    _filename_pomdp = new std::string(vm["load_pomdp"].as< std::string>());
  }
  if (vm.count("load_traj")) {
    _fileload_traj = new std::string(vm["load_traj"].as< std::string>());
  }
  if (vm.count("gene_traj")) {
    _filegene_traj = new std::string(vm["gene_traj"].as< std::string>());
  }
  if (vm.count("load_noise")) {
    _fileload_noise = new std::string(vm["load_noise"].as< std::string>());
  }
  if (vm.count("gene_noise")) {
    _filegene_noise = new std::string(vm["gene_noise"].as< std::string>());
  }
  if (vm.count("gene_esn")) {
    _filegene_esn = new std::string(vm["gene_esn"].as< std::string>());
  }
  if (vm.count("load_esn")) {
    _fileload_esn = new std::string(vm["load_esn"].as< std::string>());
  }
  if (vm.count("output")) {
    _filegene_output = new std::string(vm["output"].as< std::string>());
  }
  if (vm.count("gene_samples")) {
    _filegene_learn = new std::string(vm["gene_samples"].as< std::string>());
  }
}
// ************************************************************** load_pomdp
void load_pomdp()
{
  // free pomdp
  if( _pomdp ) delete _pomdp;
  
  // Read from file
  // std::cout << "UNSERIALZE" << std::endl;
  std::ifstream pfile( *_filename_pomdp );
  // Wrapper pour lire document
  JSON::IStreamWrapper instream(pfile);
  // Parse into a document
  rj::Document read_doc;
  read_doc.ParseStream( instream );
  pfile.close();

  _pomdp = new Model::POMDP( read_doc["pomdp"] );
}
// *************************************************************** gene_traj
void gene_traj()
{
  std::ofstream* ofile = nullptr;
  if( _filegene_traj ) {
    ofile = new std::ofstream( *_filegene_traj + ".data" );
  }
  
  // Generate seed
  unsigned int seed = utils::random::rnd_int<unsigned int>();
  gsl_rng_set( _rnd, seed );
  
  // inform traj
  if( ofile ) {
    *ofile << "## \"pomdp_name\": \"" << *_filename_pomdp << "\"," << std::endl;
    *ofile << "## \"seed\": " << seed << ", \"length\": " << _length << std::endl;
      }

  // Generate
  const std::vector<Model::Node>& list_action = _pomdp->actions();
  // Current state, obs
  unsigned int idx_state = _pomdp->cur_state()._id;
  unsigned int idx_obs = _pomdp->cur_obs()._id;
  for( unsigned int i = 0; i < _length; ++i) {
    // random action
    const Model::Node& act = list_action[gsl_rng_uniform_int( _rnd, list_action.size() )];

    unsigned int idx_next_state = _pomdp->simul_trans( act )._id;
    unsigned int idx_next_obs = _pomdp->simul_obs()._id;
    double reward = _pomdp->cur_reward();
    
    // std::cout << idx_state << "\t" << idx_obs << "\t" << act._id << "\t" << idx_next_state << "\t" << idx_next_obs << "\t" << reward << std::endl;

    if( ofile ) {
      *ofile << idx_state << "\t" << idx_obs << "\t" << act._id << "\t" << idx_next_state << "\t" << idx_next_obs << "\t" << reward << std::endl;
    }
    
    idx_state = idx_next_state;
    idx_obs = idx_next_obs;
  }

  if( ofile ) delete ofile;
}
// ***************************************************************** read_traj
void read_traj( const std::string& filename )
{
  std::ifstream ifile( filename );
  Trajectory::POMDP::read( ifile, _traj_data);
  ifile.close();

  // Check that _traj_data size is bigger than _test_length
  // before creating learn and test Data
  if( _traj_data.size() >= _test_length ) {
    _learn_data = Trajectory::POMDP::Data( _traj_data.begin(),
					   _traj_data.end() - _test_length );
    _test_data = Trajectory::POMDP::Data( _traj_data.end() - _test_length,
					  _traj_data.end() );

    //std::cout << "__READ_TRAJ : learn = " << _learn_data.size() << std::endl;
    //std::cout << "              test  = " << _test_data.size() << std::endl;
  }
  else {
    std::cerr << "__Read_Traj : error _test_length (" << _test_length << ")";
    std::cerr << " > _traj_data.size() (" << _traj_data.size() << ")" << std::endl;
    exit(2);
  }
}
// ****************************************************************** gene_esn
void gene_esn( const std::string& filename,
	       Reservoir::Tinput_size input_size = 1,
	       Layer::Toutput_size output_size = 1,   
	       Reservoir::Toutput_size reservoir_size = 10,
	       double input_scaling = 1.0,
	       double spectral_radius= 0.99,
	       double leaking_rate = 0.1
	       )
{
  free_esn();
  _res = new Reservoir( input_size, reservoir_size,
		    input_scaling, spectral_radius, leaking_rate );
  //NEW layer's input also basic input
  _lay = new Layer( input_size+reservoir_size+1, output_size );
  
  // Serialisation dans filename.json
  std::stringstream stream;
  stream << filename;
  // stream << "_" << _res_size;
  // stream << "_" << _res_scaling;
  // stream << "_" << _res_radius;
  // stream << "_" << _res_leak;
  stream << ".json";
  // std::cout << "Write ESN dans " << stream.str() << std::endl;

  rapidjson::Document doc; 
  doc.SetObject();
  doc.AddMember( "esn", _res->serialize(doc), doc.GetAllocator());
  doc.AddMember( "lay", _lay->serialize(doc), doc.GetAllocator());

  // Write to file
  std::ofstream ofile( stream.str() );
  ofile << str_obj(doc) << std::endl;
  ofile.close();
}
// ****************************************************************** read_esn
void read_esn( const std::string& filename )
{
  free_esn();
  std::ifstream ifile( filename );
  // Wrapper pour lire document
  JSON::IStreamWrapper instream(ifile);
  // Parse into a document
  rj::Document doc;
  doc.ParseStream( instream );
  ifile.close();

  _res = new Reservoir( doc["esn"] );
  _lay = new Layer( doc["lay"] );
}
// **************************************************************** load_noise
void read_noise( const std::string& filename )
{
  std::ifstream ifile( filename );
  WNoise::read( ifile, _wnoise);
  ifile.close();
}
// **************************************************************** gene_noise
void gene_noise( const std::string& filename,
		 const unsigned int length,
		 const double level,
		 const unsigned int dim)
{
  WNoise wnoise( length, level, dim );
  wnoise.create_sequence();
  _wnoise = wnoise.data();
  
  // Sauve dans JSON
  std::string fn_json = filename+".json";
  std::cout << "Write WNoise dans " << fn_json << std::endl;
  rapidjson::Document doc;
  rapidjson::Value obj = wnoise.serialize( doc );
  std::ofstream jfile( fn_json );
  jfile << str_obj(obj) << std::endl;
  jfile.close();

  // Sauve les data
  std::string fn_data = filename+".data";
  std::cout << "Write WNoise dans " << fn_data << std::endl;
  std::ofstream ofile( fn_data );

  // inform traj
  ofile << "## \"pomdp_name\": \"" << *_filename_pomdp << "\"" << std::endl;
  
  WNoise::write( ofile, _wnoise );
  ofile.close();
}
// ********************************************************************* learn
Reservoir::Tinput input_from( const Trajectory::POMDP::Item& item )
{
  // O+A neurones
  Reservoir::Tinput input(_res->input_size() );
  std::fill( input.begin(), input.end(), 0.0 );
  
  input[item.id_o] = 1.0;
  input[_pomdp->_obs.size()+item.id_a] = 1.0;

  return input;
}
/** Learn S */
// RidgeRegression::Toutput target_from( const Trajectory::POMDP::Item& item )
// {
//   // Try to learn S
//   RidgeRegression::Toutput target(_lay->output_size() );
//   std::fill( target.begin(), target.end(), 0.0 );

//   target[item.id_next_s] =1.0;

//   return target;
// }
/** Learn V(s) */
RidgeRegression::Toutput target_from( const Trajectory::POMDP::Item& item )
{
  // Try to learn V(S)
  RidgeRegression::Toutput target(_lay->output_size() );

  // V(s) = max Q(s,a) 
  target[0] = *std::max_element( _vQ[item.id_next_s].begin(),
				 _vQ[item.id_next_s].end() );

  return target;
}
void init()
{
  for( auto& item: _wnoise ) {
    // Passe dans reservoir
    auto out_res = _res->forward( item );
  }
}
void learn( double regul )
{
  // Preparation des donnees d'apprentissage et de test
  _data.clear();
  for( auto& item: _learn_data ) {
    RidgeRegression::Tinput samp_in;//( _res->input_size()+_res->output_size()+1, 0.0 );
    //DEBUG std::cout << "samp_ini=" << utils::str_vec( samp_in ) << std::endl;
	
    // Les inputs
    Reservoir::Tinput vec_in = input_from(item);
    //DEBUG std::cout << "vec_in=" << utils::str_vec(vec_in) <<  std::endl;
    // Passe dans reservoir
    auto out_res = _res->forward( vec_in );
    samp_in.insert( samp_in.begin(), out_res.begin(), out_res.end());
    //DEBUG std::cout << "samp_res=" << utils::str_vec( samp_in ) << std::endl;
    
    // Ajoute input
    samp_in.insert( samp_in.end(), vec_in.begin(), vec_in.end() );
    //DEBUG std::cout << "samp_in=" << utils::str_vec( samp_in ) << std::endl;

    // Ajoute 1.0 en bout (le neurone biais)
    samp_in.push_back( 1.0 );
    //DEBUG std::cout << "samp_final=" << utils::str_vec( samp_in ) << std::endl;
    //DEBUG std::cout << "__" << std::endl;

    RidgeRegression::Toutput vec_tar = target_from(item);
    //DEBUG std::cout << "vec_tar=" << utils::str_vec( vec_tar )  << std::endl;
    
    // Ajoute dans Data l'echantillon (entre, sortie desiree)
    _data.push_back( RidgeRegression::Sample( samp_in, vec_tar) );
  }

  //DEBUG std::cout << "___ Regression" << std::endl;
  // Ridge Regression pour apprendre la couche de sortie
  RidgeRegression reg( _res->output_size()+1, /* res output size +1 */
		       _lay->output_size(), /*target size */
		       _res->output_size() /* idx intercept */
		       );
  // Apprend, avec le meilleur coefficient de regulation
  reg.learn( _data, _lay->weights(), regul );
  // std::cout << "***** POIDS apres REGRESSION **" << std::endl;
  // std::cout << _lay->str_dump() << std::endl;

  // Les donnees d'apprentissage dans un fichier
  if( _filegene_learn ) {
    // Samples d'apprentissage
    std::string fn_sample = *_filegene_learn + "_samples.data";
    std::cout << "** Write LearnData in " << fn_sample << std::endl;
    std::ofstream ofile( fn_sample );
    // Header comments
    ofile << "## \"pomdp_name\": \"" << *_filename_pomdp << "\"," << std::endl;
    ofile << "## \"esn_name\": \"" << *_fileload_esn << "\"," << std::endl;
    ofile << "## \"traj_name\" : \"" << *_fileload_traj << "\"," << std::endl;
    if( _fileload_noise ) {
      ofile << "## \"noise_name\" : \"" << *_fileload_noise << "\"," << std::endl;
    }
    // Header ColNames
    // input
    for( unsigned int i = 0; i < _lay->input_size(); ++i) {
      ofile << "in_" << i << "\t";
    }
    // target
    for( unsigned int i = 0; i < _lay->output_size(); ++i) {
      ofile << "ta_" << i << "\t";
    }
    ofile << std::endl;
    // Data
    for( auto& sample: _data) {
      //in
      for( auto& var: sample.first) {
	ofile << var << "\t";
      }
      // target
      for( auto& var: sample.second) {
	ofile << var << "\t";
      }
      ofile << std::endl;
    }
    ofile.close();

    // Weights appris
    std::string fn_w = *_filegene_learn + "_weights.data";
    std::cout << "** Write LearnedWeights in " << fn_w << std::endl;
    std::ofstream ofile_w( fn_w );
    // Header comments
    ofile_w << "## \"pomdp_name\": \"" << *_filename_pomdp << "\"," << std::endl;
    ofile_w << "## \"esn_name\": \"" << *_fileload_esn << "\"," << std::endl;
    ofile_w << "## \"traj_name\" : \"" << *_fileload_traj << "\"," << std::endl;
    if( _fileload_noise ) {
      ofile_w << "## \"noise_name\" : \"" << *_fileload_noise << "\"," << std::endl;
    }
    ofile_w << "## \"regul\": " << regul << "," << std::endl;
    auto w = _lay->weights();
    // Header ColNames
    for( unsigned int i = 0; i < w->size2; ++i) {
      ofile_w << "inw_" << i << "\t";
    }
    ofile_w << std::endl;
    // Data
    _lay->write( ofile_w );
    ofile_w << std::endl;

    ofile_w.close();
  }
}
// ******************************************************************* predict
std::vector<RidgeRegression::Toutput>
predict( Reservoir& res,
	 Layer& lay,
	 const Trajectory::POMDP::Data& traj )
{
  // un vecteur de output
  std::vector<RidgeRegression::Toutput> result;
  
  // suppose que _mg_data a ete initialise
  for( auto& item: traj) {
    // input
    auto vec_in = input_from(item);
    // Passe dans reservoir
    auto out_res = res.forward( vec_in );
    // Ajoute input
    out_res.insert( out_res.end(), vec_in.begin(), vec_in.end() );
    // Ajoute 1.0 en bout (le neurone biais)
    out_res.push_back( 1.0 );
    // Passe dans layer
    auto out_lay = lay.forward( out_res ); 

    result.push_back( out_lay );
  }
  if(_verb)
    std::cout << "** PREDICT **" << std::endl;
  // for( auto& item: result) {
  //   std::cout << utils::str_vec(item) << std::endl;
  // }
  // // Serialisation dans filename.data
  // std::string fn_data = "data/result.data";
  // std::cout << "Write RESULTS dans " << fn_data << std::endl;
  // std::ofstream ofile( fn_data );
  // for( auto& item: result) {
  //   ofile << utils::str_vec(item) << std::endl;
  // }
  // ofile.close();

  return result;  
}
// ********************************************************************** main
int main( int argc, char *argv[] )
{
  // init random by default
  // Generate seed
  unsigned int seed = utils::random::rnd_int<unsigned int>();
  std::srand( seed );
  
  setup_options( argc, argv );

  // Charger le POMDP
  if( _filename_pomdp) {
    if( _verb )
      std::cout << "** Load POMDP from " << *_filename_pomdp << std::endl;
    load_pomdp();
  }
  // Si POMDP + nom traj => generer et sauver une trajectoire
  if( _filename_pomdp and _filegene_traj ) {
    //std::cout << "** Gene TRAJ into " << *_filegene_traj << std::endl;
    gene_traj();
  }
  // Si POMDP + gene_esn => generer et sauver un esn
  if( _filename_pomdp and _filegene_esn ) {
    //std::cout << "** Gene ESN into " << *_filegene_esn << std::endl;
    gene_esn( *_filegene_esn,
	      _pomdp->_obs.size() + _pomdp->_actions.size(), // In = O+A
	      //_pomdp->_states.size(),                    // out = S
	      1,                                          // out = V(s)
	      _res_size,                                  // _res size
	      _res_scaling, _res_radius, _res_leak
	      );
  }  
  // Si POMDP + gene_noise => generer et sauver noise
  if( _filename_pomdp and _filegene_noise ) {
    //std::cout << "** Gene NOISE into " << *_filegene_noise << std::endl;
    gene_noise( *_filegene_noise,
		_noise_length, _noise_level,
		_pomdp->_obs.size() + _pomdp->_actions.size()
		);
  }
  // Si load_traj => charger une trajectoire
  if( _fileload_traj ) {
    if( _verb ) 
      std::cout << "** Load Trajectory::POMDP::Data from " << *_fileload_traj << std::endl;
    read_traj( *_fileload_traj );
    // std::cout << "** Trajectory Read" << std::endl;
    // for( auto& item: _traj_data) {
    //   std::cout << item.id_s << ":" << item.id_o << "+" << item.id_a << "->" << item.id_next_s << ":" << item.id_next_o << " = " << item.r << std::endl;
    // }
  }
  // Si load_noise => charger un noise
  if( _fileload_noise ) {
    if( _verb )
      std::cout << "** Load WNoise::Data from " << *_fileload_noise << std::endl;
    read_noise( *_fileload_noise );
    // std::cout << "Read " << _wnoise.size() << " noise data" << std::endl;
  }
  // Si load_esn => charger un ESN
  if( _fileload_esn ) {
    if( _verb )
      std::cout << "** Load ESN from " << *_fileload_esn << std::endl;
    read_esn( *_fileload_esn );
  }

  // Si POMDP+ESN+TRAJ => learn
  if( _filename_pomdp and _fileload_esn and _fileload_traj ) {
    if( _verb )
      std::cout << "** LEARNING **" << std::endl;

    // Stocker la fonction valeur
    //_vQ = Algorithms::compute_Q( *_pomdp );
    
    // Si _noise, on commence par la 
    if( _fileload_noise ) {
      if( _verb )
	std::cout << "___ init with noise" << std::endl;
      init();
    }
    // Sauve l'etat present du reseau
    Reservoir res_after_init( *_res );
    // Apprendre => modifie _lay par regression
    if( _verb)
      std::cout << "___ learn()" << std::endl;
    learn( _regul );

    // TODO erreur commise sur le debut du reseau
    //      => sauvegarder l'etat du reseau (ce qui est deja) fait
    // un vecteur de output
    std::vector<RidgeRegression::Toutput> result_learn;
    // A partir des donnees d'apprentissage : Data
    for( auto& sample: _data) {
      // Passe dans layer
      auto out_lay = _lay->forward( sample.first ); 
      result_learn.push_back( out_lay );
    }
    
    // Prediction de la suite de la trajectoire
    if( _verb )
      std::cout << "___ predict()" << std::endl;
    std::vector<RidgeRegression::Toutput> result_test = predict( *_res, *_lay, _test_data );
    					 
    // // Premiere prediction a partir de l'etat du reseau appris
    // if( _verb ) 
    //   std::cout << "___ predict follow" << std::endl;
    // std::vector<RidgeRegression::Toutput> result_after_learn = predict( *_res, *_lay, _traj_data );
    // // Deuxieme prediction a partir de l'etat du reseau avant apprentissage
    // // mais avec _lay modifie
    // if( _verb )
    //   std::cout << "___ predict base" << std::endl;
    // std::vector<RidgeRegression::Toutput> result_after_init = predict( res_after_init, *_lay, _traj_data );
    
    // Les resultats
    // Affiche targert: \n pred\n init\n
    unsigned int idx_out = 0;
    // for( auto& item: _traj_data) {
    //   std::cout << "target:" << utils::str_vec(target_from(item))  << std::endl;
    //   std::cout << "pred:  " << utils::str_vec(result_after_learn[idx_out]) << std::endl;
    //   std::cout << "init:  " << utils::str_vec(result_after_init[idx_out]) << std::endl;
    //   idx_out ++;
    // }

    // Dans des fichiers _filegene_output+'_learn'/+'_test'
    if( _filegene_output ) {
      // Sauve les donnees
      if(_verb)
	std::cout << "** Write Output dans " << *_filegene_output << std::endl;

      // First, results on test
      std::stringstream filename_test;
      filename_test << *_filegene_output;
      filename_test << "_test";
      std::ofstream ofile( filename_test.str() );
      // Header comments
      ofile << "## \"pomdp_name\": \"" << *_filename_pomdp << "\"," << std::endl;
      ofile << "## \"traj_name\" : \"" << *_fileload_traj << "\"," << std::endl;
      ofile << "## \"esn_name\": \"" << *_fileload_esn << "\"," << std::endl;
      if( _fileload_noise ) {
	ofile << "## \"noise_name\" : \"" << *_fileload_noise << "\"," << std::endl;
      }
      ofile << "## \"regul\": " << _regul << "," << std::endl;
      ofile << "## \"test_length\": " << _test_length << "," << std::endl;
      
      // Header ColNames
      // target
      for( unsigned int i = 0; i < _lay->output_size(); ++i) {
	ofile << "ta_" << i << "\t";
      }
      // after learn
      for( unsigned int i = 0; i < _lay->output_size(); ++i) {
	ofile << "le_" << i << "\t";
      }
      // // after init
      // for( unsigned int i = 0; i < _lay->output_size(); ++i) {
      // 	ofile << "in_" << i << "\t";
      // }
      ofile << std::endl;
      // Data
      idx_out = 0;
      for( auto& item: _test_data ) { 
	// target
	for( auto& var: target_from(item)) {
	  ofile << var << "\t";
	}
	// predict
	for( auto& var: result_test[idx_out]) {
	  ofile << var << "\t";
	}
	// // init
	// for( auto& var: result_after_init[idx_out]) {
	//   ofile << var << "\t";
	// }
	ofile << std::endl;
	
	idx_out ++;
      }
      ofile.close();
      
      // Then, results on learn
      std::stringstream filename_learn;
      filename_learn << *_filegene_output;
      filename_learn << "_learn";
      ofile = std::ofstream( filename_learn.str() );
      // Header comments
      ofile << "## \"pomdp_name\": \"" << *_filename_pomdp << "\"," << std::endl;
      ofile << "## \"traj_name\" : \"" << *_fileload_traj << "\"," << std::endl;
      ofile << "## \"esn_name\": \"" << *_fileload_esn << "\"," << std::endl;
      if( _fileload_noise ) {
	ofile << "## \"noise_name\" : \"" << *_fileload_noise << "\"," << std::endl;
      }
      ofile << "## \"regul\": " << _regul << "," << std::endl;
      ofile << "## \"test_length\": " << _test_length << "," << std::endl;
      
      // Header ColNames
      // target
      for( unsigned int i = 0; i < _lay->output_size(); ++i) {
	ofile << "ta_" << i << "\t";
      }
      // after learn
      for( unsigned int i = 0; i < _lay->output_size(); ++i) {
	ofile << "le_" << i << "\t";
      }
      // // after init
      // for( unsigned int i = 0; i < _lay->output_size(); ++i) {
      // 	ofile << "in_" << i << "\t";
      // }
      ofile << std::endl;
      // Data
      idx_out = 0;
      for( auto& item: _learn_data ) { 
	// target
	for( auto& var: target_from(item)) {
	  ofile << var << "\t";
	}
	// predict
	for( auto& var: result_learn[idx_out]) {
	  ofile << var << "\t";
	}
	// // init
	// for( auto& var: result_after_init[idx_out]) {
	//   ofile << var << "\t";
	// }
	ofile << std::endl;
      
	idx_out ++;
      }
      ofile.close();
    }
  
  }
  
  free_mem();
  free_esn();
  return 0;
}
