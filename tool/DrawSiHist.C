void DrawSiHist(){
    
    const char* hname[6] = {"fadc","madc1","madc2","madc3","madc4","madc5"};

    if(!gFile){
        cout << "No root file opened!" << endl;
        return;
    }

    TString fullpath = gFile->GetName();
    TString fname = gSystem->BaseName(fullpath); 
    fname = fname(0, fname.Last('.') ); 

    TCanvas *c = new TCanvas("c",fname,1200,900);
    c->Divide(2,3);

    for(int i=0;i<6;i++){
        c->cd(i+1);
        TH2D *h = (TH2D*)gFile->Get(hname[i]);
        if(!h){
            cout<<"not found: "<<hname[i]<<endl;
            continue;
        }
        h->Draw();
        gPad->SetLogz();    
    }
    c->Update();
}
