void DrawTdcHist(){
  
  if(!gFile){
    cout << "No root file opend!" << endl;
    return;
  }
  
  TTree*tree = (TTree*)gFile->Get("tree");
  if (!tree){
    cout << "Tree not found" << endl;
  }
  
  TString fullpath = gFile->GetName();
  TString fname = gSystem->BaseName(fullpath);
  fname = fname(0, fname.Last('.'));
  
  TCanvas *c = new TCanvas("",fname,1200,900);
  c->Divide(4,4);

  for(int i = 0; i < 16;i++){
    c->cd(i+1);
    tree->Draw(Form("TDC_sub_Trig[%d]*0.1>>h1(300,-2000,1000)",224+i));
    TH1D* h = (TH1D*)gDirectory->Get("h1");
    h->GetXaxis()->SetLabel("ns");
  }
  c->Update(); 
      
  
  
  
}
