#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "TChain.h"
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TObjString.h"
#include "TSystem.h"
#include "TROOT.h"

#include "TMVA/Factory.h"
#include "TMVA/DataLoader.h"
#include "TMVA/Tools.h"
#include "TMVA/TMVAGui.h"

void addHadVariable(TMVA::DataLoader *dataloader) {
  dataloader->AddVariable("Ks_d",            'F');
  dataloader->AddVariable("Ks_pt",           'F');
  dataloader->AddVariable("Ks_eta",          'F');
  dataloader->AddVariable("Ks_phi",          'F');
  //dataloader->AddVariable("Ks_mass",         'F');
  dataloader->AddVariable("Ks_lxy",          'F');
  dataloader->AddVariable("Ks_lxySig",       'F');
  dataloader->AddVariable("Ks_l3D",          'F');
  dataloader->AddVariable("Ks_l3DSig",       'F');
  dataloader->AddVariable("Ks_legDR",        'F');
  dataloader->AddVariable("Ks_angleXY",      'F');
  dataloader->AddVariable("Ks_angleXYZ",     'F');
  dataloader->AddVariable("Ks_chi2",         'F');
  dataloader->AddVariable("Ks_dca",          'F');
  dataloader->AddVariable("Ks_dau1_chi2",    'F');
  dataloader->AddVariable("Ks_dau1_ipsigXY", 'F');
  dataloader->AddVariable("Ks_dau1_ipsigZ",  'F');
  dataloader->AddVariable("Ks_dau1_pt",      'F');
  dataloader->AddVariable("Ks_dau2_chi2",    'F');
  dataloader->AddVariable("Ks_dau2_ipsigXY", 'F');
  dataloader->AddVariable("Ks_dau2_ipsigZ",  'F');
  dataloader->AddVariable("Ks_dau2_pt",      'F');
}

void addTree(TMVA::DataLoader *dataloader, TTree* sig, TTree* bkg, Double_t sigW = 1.0, Double_t bkgW = 1.0) {
  dataloader->AddSignalTree    ( sig, sigW );
  dataloader->AddBackgroundTree( bkg, bkgW );
}
void addTree(TMVA::DataLoader *dataloader, TTree* sigTrain, TTree* sigTest, TTree* bkgTrain, TTree* bkgTest, Double_t sigWTrain = 1.0, Double_t sigWTest = 1.0, Double_t bkgWTrain = 1.0, Double_t bkgWTest = 1.0) {
  dataloader->AddSignalTree    ( sigTrain, sigWTrain, "Training");
  dataloader->AddSignalTree    ( sigTest,  sigWTest,  "Test");
  dataloader->AddBackgroundTree( bkgTrain, bkgWTrain, "Training");
  dataloader->AddBackgroundTree( bkgTest,  bkgWTest,  "Test");
}


void addMethod(std::map<std::string,int> Use, TMVA::Factory *factory, TMVA::DataLoader *dataloader) {
  if (Use["MLPBFGS"])
    factory->BookMethod( dataloader, TMVA::Types::kMLP, "MLPBFGS", "H:!V:NeuronType=tanh:VarTransform=N:NCycles=600:HiddenLayers=N+5:TestRate=5:TrainingMethod=BFGS:!UseRegulator" );
  if (Use["TMlpANN"])
    factory->BookMethod( dataloader, TMVA::Types::kTMlpANN, "TMlpANN", "!H:!V:NCycles=200:HiddenLayers=N+1,N:LearningMethod=BFGS:ValidationFraction=0.3"  );
  if (Use["BDTG"]) // Gradient Boost
    factory->BookMethod( dataloader, TMVA::Types::kBDT, "BDTG",
                         "!H:!V:NTrees=1000:MinNodeSize=2.5%:BoostType=Grad:Shrinkage=0.10:UseBaggedBoost:BaggedSampleFraction=0.5:nCuts=20:MaxDepth=2" );
  if (Use["BDT"])  // Adaptive Boost
    factory->BookMethod( dataloader, TMVA::Types::kBDT, "BDT",
                         "!H:!V:NTrees=850:MinNodeSize=2.5%:MaxDepth=3:BoostType=AdaBoost:AdaBoostBeta=0.5:UseBaggedBoost:BaggedSampleFraction=0.5:SeparationType=GiniIndex:nCuts=20" );
}

int vts_dR_04_Had( TString myMethodList = "" )
{
  // This loads the library
  TMVA::Tools::Instance();

  // Default MVA methods to be trained + tested
  std::map<std::string,int> Use;

  Use["MLPBFGS"]         = 0; // Recommended ANN with optional training method
  Use["TMlpANN"]         = 0; // ROOT's own ANN
  // Boosted Decision Trees
  Use["BDT"]             = 1; // uses Adaptive Boost
  Use["BDTG"]            = 0; // uses Gradient Boost
  //
  // ---------------------------------------------------------------

  std::cout << std::endl;
  std::cout << "==> Start vts_dR_04_Had" << std::endl;

  // Select methods (don't look at this code - not of interest)
  if (myMethodList != "") {
    for (std::map<std::string,int>::iterator it = Use.begin(); it != Use.end(); it++) it->second = 0;

    std::vector<TString> mlist = TMVA::gTools().SplitString( myMethodList, ',' );
    for (UInt_t i=0; i<mlist.size(); i++) {
      std::string regMethod(mlist[i]);
      if (Use.find(regMethod) == Use.end()) {
        std::cout << "Method \"" << regMethod << "\" not known in TMVA under this name. Choose among the following:" << std::endl;
        for (std::map<std::string,int>::iterator it = Use.begin(); it != Use.end(); it++) std::cout << it->first << " ";
        std::cout << std::endl;
        return 1;
      }
      Use[regMethod] = 1;
    }
  }

  // --------------------------------------------------------------------------------------------------

  // Here the preparation phase begins

  // Read training and test data
  // (it is also possible to use ASCII format as real_vs_fake_highest -> see TMVA Users Guide)
  TFile *bbars_pythia(0);             TFile *bbars_herwig(0);
  TFile *bsbar_pythia(0);             TFile *bsbar_herwig(0);
  TFile *bbars_bsbar_pythia(0);       TFile *bbars_bsbar_herwig(0);
  TFile *bbars_bsbar_bbbar_pythia(0);

  TString sample_bbars_pythia             = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bbars_2l_FxFx_sum_349.root";
  TString sample_bbars_herwig             = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bbars_2l_FxFx_herwigpp_sum_49.root";

  TString sample_bsbar_pythia             = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bsbar_2l_FxFx_sum_350.root";
  TString sample_bsbar_herwig             = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bsbar_2l_FxFx_herwigpp_sum_49.root";

  TString sample_bbars_bsbar_pythia       = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bbars_bsbar_sum_349_350.root";
  TString sample_bbars_bsbar_herwig       = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bbars_bsbar_herwigpp_sum_49.root";

  TString sample_bbars_bsbar_bbbar_pythia = "/xrootd/store/user/wjjang/test/sum_tt012j/20190304/tt012j_bbars_bsbar_bbbar_sum_349_350_201.root";//"/xrootd/store/user/wjjang/test/sum_tt012j/20190105/tt012j_bbars_bsbar_bbbar_sum_349_350_201.root";

  bbars_pythia = TFile::Open( sample_bbars_pythia );                          //bbars_herwig = TFile::Open( sample_bbars_herwig );
  bsbar_pythia = TFile::Open( sample_bsbar_pythia );                          //bsbar_herwig = TFile::Open( sample_bsbar_herwig );
  bbars_bsbar_pythia = TFile::Open( sample_bbars_bsbar_pythia );              //bbars_bsbar_herwig = TFile::Open( sample_bbars_bsbar_herwig );
  bbars_bsbar_bbbar_pythia = TFile::Open( sample_bbars_bsbar_bbbar_pythia );

  std::cout << "--- vts_dR_04_Had       : Using input file 1 : " << bbars_pythia->GetName() << std::endl;
//  std::cout << "--- vts_dR_04_Had       : Using input file 2 : " << bbars_herwig->GetName() << std::endl;
  std::cout << "--- vts_dR_04_Had       : Using input file 3 : " << bsbar_pythia->GetName() << std::endl;
//  std::cout << "--- vts_dR_04_Had       : Using input file 4 : " << bsbar_herwig->GetName() << std::endl;
  std::cout << "--- vts_dR_04_Had       : Using input file 5 : " << bbars_bsbar_pythia->GetName() << std::endl;
//  std::cout << "--- vts_dR_04_Had       : Using input file 6 : " << bbars_bsbar_herwig->GetName() << std::endl;
  std::cout << "--- vts_dR_04_Had       : Using input file 7 : " << bbars_bsbar_bbbar_pythia->GetName() << std::endl;

  // Register the training and test trees
  TTree *bbars_pythia_tree           = (TTree*)bbars_pythia->Get("MVA_had");
//  TTree *bbars_herwig_tree           = (TTree*)bbars_herwig->Get("MVA_had");
  
  TTree *bsbar_pythia_tree           = (TTree*)bsbar_pythia->Get("MVA_had");
//  TTree *bsbar_herwig_tree           = (TTree*)bsbar_herwig->Get("MVA_had");

  TTree *bbars_bsbar_pythia_tree     = (TTree*)bbars_bsbar_pythia->Get("MVA_had");
//  TTree *bbars_bsbar_herwig_tree     = (TTree*)bbars_bsbar_herwig->Get("MVA_had");

  TTree *bbars_bsbar_bbbar_pythia_tree = (TTree*)bbars_bsbar_bbbar_pythia->Get("MVA_had");

  // Create a ROOT output file where TMVA will store ntuples, histograms, etc.
  TString outfileName( "./output/vts_dR_04_Had.root" );
  TFile* outputFile = TFile::Open( outfileName, "RECREATE" );

  TMVA::Factory *factory = new TMVA::Factory( "vts_dR_04_Had", outputFile,
                                              "!V:!Silent:Color:DrawProgressBar:Transformations=I;D;P;G,D:AnalysisType=Classification" );

  TMVA::DataLoader *pp_real_vs_fake = new TMVA::DataLoader("pp_real_vs_fake");

  //    (TMVA::gConfig().GetVariablePlotting()).fTimesRMS = 8.0;
  //    (TMVA::gConfig().GetIONames()).fWeightFileDir = "myWeightDirectory";
  (TMVA::gConfig().GetVariablePlotting()).fMaxNumOfAllowedVariablesForScatterPlots = 99999;
  (TMVA::gConfig().GetVariablePlotting()).fNbinsXOfROCCurve = 1000;

  Double_t signalWeight     = 1.0; Double_t backgroundWeight = 1.0;

  TCut cut_real = "Ks_nMatched == 2";
  TCut cut_fake = "Ks_nMatched != 2";
  // It's for 20181120 samples
  TString opt_pp = "nTrain_Signal=12000:nTrain_Background=870000:SplitMode=Random:NormMode=NumEvents:!V";

  addHadVariable(pp_real_vs_fake);
  addTree(pp_real_vs_fake,   bbars_bsbar_bbbar_pythia_tree, bbars_bsbar_bbbar_pythia_tree, signalWeight, backgroundWeight);
  pp_real_vs_fake->PrepareTrainingAndTestTree( cut_real, cut_fake, opt_pp ); 
  addMethod(Use, factory, pp_real_vs_fake);

  // For an example of the category classifier usage, see: vts_dR_04_HadCategory
  //
  // --------------------------------------------------------------------------------------------------
  //  Now you can optimize the setting (configuration) of the MVAs using the set of training events
  // STILL EXPERIMENTAL and only implemented for BDT's !
  //
  //     factory->OptimizeAllMethods("SigEffAt001","Scan");
  //     factory->OptimizeAllMethods("ROCIntegral","FitGA");
  //
  // --------------------------------------------------------------------------------------------------

  // Now you can tell the factory to train, test, and evaluate the MVAs
  //
  // Train MVAs using the set of training events
  factory->TrainAllMethods();

  // Evaluate all MVAs using the set of test events
  factory->TestAllMethods();

  // Evaluate and compare performance of all configured MVAs
  factory->EvaluateAllMethods();

  // --------------------------------------------------------------

  // Save the output
  outputFile->Close();

  std::cout << "==> Wrote root file: " << outputFile->GetName() << std::endl;
  std::cout << "==> vts_dR_04_Had is done!" << std::endl;

  delete factory;
  delete pp_real_vs_fake;

  // Launch the GUI for the root macros
  if (!gROOT->IsBatch()) TMVA::TMVAGui( outfileName );

  return 0;
}

int main( int argc, char** argv )
{
  // Select methods (don't look at this code - not of interest)
  TString methodList;
  for (int i=1; i<argc; i++) {
    TString regMethod(argv[i]);
    if(regMethod=="-b" || regMethod=="--batch") continue;
    if (!methodList.IsNull()) methodList += TString(",");
    methodList += regMethod;
  }
  return vts_dR_04_Had(methodList);
}
