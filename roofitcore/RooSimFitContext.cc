/*****************************************************************************
 * Project: BaBar detector at the SLAC PEP-II B-factory
 * Package: RooFitCore
 *    File: $Id: RooSimFitContext.cc,v 1.3 2001/08/03 02:04:33 verkerke Exp $
 * Authors:
 *   DK, David Kirkby, Stanford University, kirkby@hep.stanford.edu
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu
 * History:
 *   07-Mar-2001 WV Created initial version
 *
 * Copyright (C) 2001 University of California
 *****************************************************************************/

#include "RooFitCore/RooSimFitContext.hh"
#include "RooFitCore/RooSimultaneous.hh"
#include "RooFitCore/RooAbsCategoryLValue.hh"
#include "RooFitCore/RooDataSet.hh"
#include "RooFitCore/RooFormulaVar.hh"

ClassImp(RooSimFitContext)
;

RooSimFitContext::RooSimFitContext(const RooDataSet* data, const RooSimultaneous* simpdf) : RooFitContext(data,simpdf,kFALSE,kTRUE)
{
  RooAbsCategoryLValue& simCat = (RooAbsCategoryLValue&) simpdf->_indexCat.arg() ;

  _nCtx = simCat.numTypes() ;
  _nCtxFilled = 0 ;
  _ctxArray = new pRooFitContext[_nCtx] ;
  _offArray = new Double_t[_nCtx] ;
  _nllArray = new Double_t[_nCtx] ;
  _dirtyArray = new Bool_t[_nCtx] ;

  TString simCatName(simCat.GetName()) ;
  
  // Create array of regular fit contexts, containing subset of data and single fitCat PDF
  Int_t n(0) ;
  RooCatType* type ;
  TIterator* catIter = simCat.typeIterator() ;
  while(type=(RooCatType*)catIter->Next()){
    simCat.setIndex(type->getVal()) ;

    // Retrieve the PDF for this simCat state
    RooRealProxy* proxy = (RooRealProxy*) simpdf->_pdfProxyList.FindObject((const char*) simpdf->_indexCat) ;
    if (proxy) {
      cout << "RooSimFitContext::RooSimFitContext: creating fit sub-context for state " << type->GetName() << endl ;
      RooAbsPdf* pdf = (RooAbsPdf*)proxy->absArg() ;

      //Refine a dataset containing only events for this simCat state
      char cutSpec[1024] ;
      sprintf(cutSpec,"%s==%d",simCatName.Data(),simCat.getIndex()) ;
      RooDataSet* dset = new RooDataSet("dset_simcat","dset_simcat",
					_dataClone,*_dataClone->get(),RooFormulaVar("simCatCut",cutSpec,simCat)) ;
      
      _ctxArray[n] = new RooFitContext(dset,pdf,kFALSE,kTRUE) ;
      _dirtyArray[n] = kTRUE ;
      _nCtxFilled++ ;

    } else {
      cout << "RooSimFitContext::RooSimFitContext: no pdf for state " << type->GetName() << endl ;
      simpdf->_pdfProxyList.Print() ;
      _ctxArray[n] = 0 ;
      _dirtyArray[n] = kFALSE ;
    }

    _nllArray[n] = 0 ;
    n++ ;
  }

  // Precalculate NLL offsets for each category
  Int_t i ;
  Double_t logNF = log(_nCtxFilled) ;
  for (i=0 ; i<_nCtx ; i++) {
    if (_ctxArray[i]) {
      _offArray[i] = _ctxArray[i]->_dataClone->GetEntries()*logNF ;
    } else {
      _offArray[i] = 0 ;
    }
  }

  delete catIter ;

}


RooSimFitContext::~RooSimFitContext() 
{
  Int_t i ;
  for (i=0 ; i<_nCtx ; i++) {
    RooDataSet* dset = _ctxArray[i]->data() ;
    if (_ctxArray[i]) delete _ctxArray[i] ;
    delete dset ;
  }

  delete[] _ctxArray ;
  delete[] _nllArray ;
  delete[] _offArray ;
  delete[] _dirtyArray ;
}



Double_t RooSimFitContext::nLogLikelihood(Bool_t dummy) const 
{
  Double_t nllSum(0) ;
  // Update likelihood from subcontexts that changed
  Double_t offSet(log(_nCtxFilled)) ;
  Int_t i ;
  for (i=0 ; i<_nCtx ; i++) {
    if (_ctxArray[i]) {
      if (_dirtyArray[i]) {
	_nllArray[i] = _ctxArray[i]->nLogLikelihood() + _offArray[i] ;
	_dirtyArray[i] = kFALSE ;
      }
      nllSum += _nllArray[i] ;
    }
  }
  
  // Return sum of NLL from subcontexts
  return nllSum ;
}


Bool_t RooSimFitContext::optimize(Bool_t doPdf,Bool_t doData, Bool_t doCache) 
{
  Bool_t ret(kFALSE) ;
  Int_t i ;
  for (i=0 ; i<_nCtx ; i++) {
    cout << "RooSimFitContext::optimize: forwarding call to subContext " << i << endl ;
    if (_ctxArray[i]) {
      if (_ctxArray[i]->optimize(doPdf,doData,doCache)) ret=kTRUE ;
    }
  }
  return ret ;
}


Bool_t RooSimFitContext::setPdfParamVal(Int_t index, Double_t value, Bool_t verbose) 
{
  Int_t i ;
  // First check if variable actually changes 
  if (!RooFitContext::setPdfParamVal(index,value,verbose)) return kFALSE;

  // Forward parameter change to sub-contexts
  for (i=0 ; i<_nCtx ; i++) {
    if (_ctxArray[i]) {
      TObject* par = _paramList->At(index) ;
      if (!par) {
	cout << "RooSimFitContext::setPdfParamVal: cannot find parameter with index " << index << endl ;
	assert(0) ;
      }      
      Int_t subIdx = _ctxArray[i]->_paramList->IndexOf(par) ;

      // Mark subcontexts as dirty
      if (subIdx!=-1) {	  
	_ctxArray[i]->setPdfParamVal(subIdx,value,kFALSE) ;
	_dirtyArray[i] = kTRUE ;
      }
    }
  }

  return kTRUE ;
}

