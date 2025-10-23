void DrawAllHistograms(const char* _filename = nullptr, const char* opt = nullptr)
{  

  if(_filename){
    if(gDirectory &&
       std::string(gDirectory->GetName()) != "Rint")
    {
      gDirectory->Close();
    }
    new TFile(_filename);
  }

  std::vector<std::pair<std::string,TH1*>> hists;
  auto keys  = gDirectory->GetListOfKeys();
  unsigned int nKeys = keys->GetSize();

  for(int iKey = 0; iKey < nKeys; ++iKey){
    std::string name = keys->At(iKey)->GetName();
    auto h = dynamic_cast<TH1*>(gDirectory->Get(name.c_str()));
    if(!h)
      continue;
    else
      hists.push_back(std::make_pair(name, h));
  }


  auto objs  = gDirectory->GetList();
  unsigned int nObjs = objs->GetSize();

  for(int iObj = 0; iObj < nObjs; ++iObj){
    std::string name = objs->At(iObj)->GetName();
    auto h = dynamic_cast<TH1*>(gDirectory->Get(name.c_str()));
    if(!h)
      continue;
    else if(std::find_if(hists.begin(), hists.end(), [&name](const std::pair<std::string, TH1*>  &a){return a.first == name;}) != hists.end())
      continue;
    else
      hists.push_back(std::make_pair(name, h));
  }


  

  std::cout << "[p, P]       : Go to Previous histogram" <<std::endl;
  std::cout << "[n, N]       : Go to Next histogram" <<std::endl;
  std::cout << "[+/-integer] : Go to histogram" <<std::endl;
  std::cout << "[Else]       : break" <<std::endl; 

  int index = 0;
  while(0 <= index &&
	index < hists.size() ){
    
    hists.at(index).second->Draw(opt);
    gPad->Update();
    std::string preName, postName;
    std::string name = hists.at(index).first;

    try{
      preName = hists.at(index-1).first;
    }
    catch(...){
      preName = " --- ";
    }

    try{
      postName = hists.at(index+1).first;
    }
    catch(...){
      postName = " --- ";
    }
    std::string tmp;
    std::cout << preName << " <<--[P]-- " <<name << " --[N]-->> " << postName <<std::endl;


    std::cin >> tmp;
    int step = 0;
    try{
      step = std::stoi(tmp);
    }
    catch(...){
      if(tmp == "N" ||
	 tmp == "n" ){
	  step = 1;
      }
      else if(tmp == "P" ||
	      tmp == "p"){
	  step = -1;
      }
      else
	step = hists.size();
    }

    
    index += step;
    
  }
    
  
}
