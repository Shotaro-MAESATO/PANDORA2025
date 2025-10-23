// fit  gaus(),  gaus(0)+gaus(3)

int N_FADC_BOARD = 3;
int N_CH = 16;
int hist_xmin = 1000;
int hist_xmax = 5000;

void FitAlphaSource(const TString& input_file){

    TFile* fin = TFile::Open(input_file);
    if(!fin || fin->IsZombie()){
        cerr<< " Error: Failed to open file" << input_file << endl;
        return;
    }

    ofstream fout("calib/peakmean.txt");
    
    for(int i=0;i<N_FADC_BOARD;++i){
      TCanvas* c = new TCanvas(Form("Board%d",i),Form("Board%d",i),800, 800);
      c->Divide(4,4);
      
      for(int j=0;j<N_CH;++j){
	c->cd(j+1);
	TH1D* h = (TH1D*)fin->Get(Form("fadc%d_%02d",i,j));
	if(!h){
	  cerr << "Error; Failed to get histogram : " << Form("fadc%d_%02d",i,j) <<endl;
	  continue;
	}
	h->GetXaxis()->SetRangeUser(hist_xmin, hist_xmax);
	
	TSpectrum* spec = new TSpectrum(3);
	spec->Search(h,2,"new", 0.05); //(histname　pointa, sigma, option, threshold);
	int nPeaks = spec->GetNPeaks();
	if(nPeaks != 3){
	  cerr << "Error: Number of peaks is not 3 in histogram " << Form("fadc%d_%02d",i,j) << endl;
	  continue;
	}
	
	vector<double> peakposition;
	for(int k = 0; k < nPeaks; ++k)peakposition.push_back(spec->GetPositionX()[k]);
	sort(peakposition.begin(), peakposition.end());

	vector<double> peakpositionY;
	for(int k = 0; k < nPeaks; ++k)peakpositionY.push_back(spec->GetPositionY()[k]);
	Double_t  Yave = (peakpositionY[0] + peakpositionY[1] + peakpositionY[2])/3.;
	
	TF1 *fg  = new TF1("fg","gaus");
	TF1 *f2g = new TF1("f2g","gaus(0)+gaus(3)");
	h->Fit("fg","","",peakposition[0]-150,peakposition[0]+150);
	fg->SetLineColor(kRed);
	
	//when you can't fit well, you can set parameters manually
	f2g->SetParameters(Yave , peakposition[1], 50 , Yave ,peakposition[2], 50);   
	h->Fit("f2g","+","",peakposition[1]-250,peakposition[2]+250);
	h->Draw();
	c->Update();
	
//	TCanvas* cp = new TCanvas();
//	h->Draw();
//	cp->SaveAs(Form("calib/fig/fadc%d_%02d.png", i, j));
//	delete cp;
	
	double mean[3]= {0, 0, 0};
	mean[0] = fg->GetParameter(1);
	mean[1] = f2g->GetParameter(1);
	mean[2] = f2g->GetParameter(4); 
	fout << mean[0] << " " << mean[1] << " " << mean[2] << endl;
	
      }

      
    }
    fout.close();
    
}
