#include <iostream>
#include <iomanip>
#include <sstream>
#include <array>
#include <vector>
#include <functional>
#include <chrono>
#include <random>
#include <future>
#include <cstring>
#include <cstdio>

#include <fastjet/ClusterSequence.hh>

using namespace std;
using namespace fastjet;

string exec(const char* cmd) {
  FILE* pipe = popen(cmd, "r");
  if (!pipe) return "ERROR";
  char buffer[128];
  std::string result = "";
  while(!feof(pipe)) {
  	if(fgets(buffer, 128, pipe) != NULL)
  		result += buffer;
  }
  pclose(pipe);
  return result;
}

// read 4-momentum from input stream
istream& operator>>(istream &in, vector<PseudoJet>& p) {
  double px, py, pz, E;
  in >> px >> py >> pz >> E;
  if (in) p.emplace_back(px,py,pz,E);
  return in;
}

int main(int argc, char **argv)
{
  if (argc!=3 && argc!=4) {
    cout << "Usage: " << argv[0] << " algorithm radius N" << endl;
    return 1;
  }

  const int np = (argc>3 ? atoi(argv[3]) : 0);

  stringstream cmd_n2;
  cmd_n2 << "java -classpath .. Test " << argv[1] << ' ' << argv[2];

  const double R = atof(argv[2]);
  JetAlgorithm jalg;
  function<double(const PseudoJet& a, const PseudoJet& b)> dij;
  function<double(const PseudoJet& a)> diB;

  if (!strcmp(argv[1],"antikt")) {
    jalg = antikt_algorithm;
    dij = [R](const PseudoJet& a, const PseudoJet& b){
      return min(1./a.kt2(),1./b.kt2())*a.delta_R(b)/R;
    };
    diB = [](const PseudoJet& a){ return 1./a.kt2(); };
  } else if (!strcmp(argv[1],"kt")) {
    jalg = kt_algorithm;
    dij = [R](const PseudoJet& a, const PseudoJet& b){
      return min(a.kt2(),b.kt2())*a.delta_R(b)/R;
    };
    diB = [](const PseudoJet& a){ return a.kt2(); };
  } else if (!strcmp(argv[1],"cambridge")) {
    jalg = cambridge_algorithm;
    dij = [R](const PseudoJet& a, const PseudoJet& b){
      return a.delta_R(b)/R;
    };
    diB = [](const PseudoJet& a){ return 1.; };
  } else {
    cerr << "Undefined algorithm " << argv[1] << endl;
    return 1;
  }
  
  JetDefinition jdef(jalg,R);
  
  // mersenne twister random number generator
  mt19937 gen(chrono::system_clock::now().time_since_epoch().count());
  uniform_real_distribution<double> dist(0.0,1.0);

  long long cnt = 0;
  while (true) {
    stringstream ps;
    string ps_str;
  
    // Generate 2 random 4-vectors
    if (np) {
      ps << fixed << scientific << setprecision(8);
      for (int i=0, n=4*np; i<n; ++i) ps << ' ' << dist(gen);
      ps_str = ps.str();
    } else {
      while (cin >> ps_str) { ps <<' '<< ps_str; }
      ps_str = ps.str();
    }

    // FastJet **********************************************
    vector<PseudoJet> particles;

    auto fut_fj = async(launch::async, [&ps,&jdef,&particles](){
      // collect particles
      while (ps >> particles) { };

      // define ClusterSequence
      ClusterSequence seq(particles, jdef, false);

      // cluster jets
      vector<PseudoJet> jets = seq.inclusive_jets();

      // sort jets by pT
      sort( jets.begin(), jets.end(),
        [](const PseudoJet& i, const PseudoJet& j)
          { return i.pt() > j.pt(); }
      );

      stringstream ss_fj;
      ss_fj << fixed << scientific << setprecision(8);
      for (auto& jet : jets) ss_fj << jet.pt() << ' ';
      return ss_fj.str();
    });

    // n2jet_java *******************************************

    auto fut_n2 = async(launch::async, [&cmd_n2,&ps_str](){
      return exec((cmd_n2.str()+ps_str).c_str());
    });

    // Print output *****************************************
    
    const string out_fj = fut_fj.get();
    const string out_n2 = fut_n2.get();

    if (out_n2!=out_fj || !np) {
      cout << "FJ: " << out_fj << '\n'
           << "N2: " << out_n2 << endl;
        for (size_t i=0; i<particles.size(); ++i) {
          cout << "p"<<i<<": " << ps_str.substr(i*60+1,59)
               << " diB = " << diB(particles[i]) << endl;
        }
        for (size_t i=1; i<particles.size(); ++i)
          for (size_t j=0; j<i; ++j)
            cout << "d" << setw(3) << i << setw(3) << j
                 << " = " << dij(particles[i],particles[j]) << endl;
        cout << endl;
    } else {
      for (int i=0;i<9;++i) cout << '\b';
      cout << setw(9) << ++cnt;
      cout.flush();
    }
    
    if (!np) break;
  }

  return 0;
}
