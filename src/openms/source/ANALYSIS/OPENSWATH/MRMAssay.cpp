// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2015.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: George Rosenberger $
// $Authors: George Rosenberger $
// --------------------------------------------------------------------------

#include <OpenMS/ANALYSIS/OPENSWATH/MRMAssay.h>

namespace OpenMS
{
  MRMAssay::MRMAssay()
  {
  }

  MRMAssay::~MRMAssay()
  {
  }

  std::vector<std::string> MRMAssay::getMatchingPeptidoforms_(const double fragment_ion, std::vector<std::pair<double, std::string> >& ions, const double mz_threshold)
  {
    std::vector<std::string> isoforms;

    for (std::vector<std::pair<double, std::string> >::iterator i_it = ions.begin(); i_it != ions.end(); ++i_it)
    {
      if (i_it->first - mz_threshold <= fragment_ion && i_it->first + mz_threshold >= fragment_ion)
      {
        isoforms.push_back(i_it->second);
      }
    }

    std::sort(isoforms.begin(), isoforms.end());
    isoforms.erase(std::unique(isoforms.begin(), isoforms.end()), isoforms.end());

    return isoforms;
  }

  int MRMAssay::getSwath_(const std::vector<std::pair<double, double> > swathes, const double precursor_mz)
  {
    int swath = -1;

    // we go through all swaths in ascending order and if the transitions falls in overlap, only the upper swath will be used and checked.
    for (std::vector<std::pair<double, double> >::const_iterator it = swathes.begin(); it != swathes.end(); ++it)
    {
      if (precursor_mz >= it->first && precursor_mz <= it->second)
      {
        swath = it - swathes.begin();
      }
    }

    if (swath != -1)
    {
      return swath;
    }
    else
    {
      return -1;
    }
  }

  bool MRMAssay::isInSwath_(const std::vector<std::pair<double, double> > swathes, const double precursor_mz, const double product_mz)
  {
    int swath_idx = getSwath_(swathes, precursor_mz);

    if (swath_idx == -1) { return true; } // remove all transitions that are not in swath range
    else
    {
      std::pair<double, double> swath = swathes[getSwath_(swathes, precursor_mz)];

      if (product_mz >= swath.first && product_mz <= swath.second)
      {
        return true;
      }
      else { return false; }
    }
  }

  void MRMAssay::addModification_(std::vector<TargetedExperiment::Peptide::Modification>& mods,
                                  int location, ResidueModification& rmod)
  {
    TargetedExperiment::Peptide::Modification mod;
    String unimod_str = rmod.getUniModAccession();
    mod.location = location;
    mod.mono_mass_delta = rmod.getDiffMonoMass();
    mod.avg_mass_delta = rmod.getDiffAverageMass();
    // CV term with the full unimod accession number and name
    CVTerm unimod_name;
    unimod_name.setCVIdentifierRef("UNIMOD");
    unimod_name.setAccession(unimod_str.toUpper());
    unimod_name.setName(rmod.getName());
    mod.addCVTerm(unimod_name);
    mods.push_back(mod);
  }

  std::string MRMAssay::getRandomSequence_(size_t sequence_size, boost::variate_generator<boost::mt19937&, boost::uniform_int<> >
                                           pseudoRNG)
  {
    std::string aa[] =
    {
      "A", "N", "D", "C", "E", "Q", "G", "H", "I", "L", "M", "F", "S", "T", "W",
      "Y", "V"
    };
    size_t aa_size = 17;

    std::string peptide_sequence = "";

    size_t pos;
    for (size_t i = 0; i < sequence_size; ++i)
    {
      pos = (pseudoRNG() % aa_size);
      peptide_sequence += aa[pos];
    }

    return peptide_sequence;
  }

  std::vector<std::vector<size_t> > MRMAssay::nchoosekcombinations_(std::vector<size_t> n, size_t k)
  {
    std::vector<std::vector<size_t> > combinations;

    std::string bitmask(k, 1);
    bitmask.resize(n.size(), 0);

    do
    {
      std::vector<size_t> combination;
      for (size_t i = 0; i < n.size(); ++i)
      {
        if (bitmask[i])
        {
          combination.push_back(n[i]);
        }
      }
      combinations.push_back(combination);
    }
    while (std::prev_permutation(bitmask.begin(), bitmask.end()));

    return combinations;
  }

  std::vector<OpenMS::AASequence> MRMAssay::addModificationsSequences_(std::vector<OpenMS::AASequence> sequences, std::vector<std::vector<size_t> > mods_combs, OpenMS::String modification)
  {
    std::vector<OpenMS::AASequence> modified_sequences;
    bool multi_mod_switch = false;

    for (std::vector<OpenMS::AASequence>::iterator sq_it = sequences.begin(); sq_it != sequences.end(); ++sq_it)
    {
      for (std::vector<std::vector<size_t> >::iterator mc_it = mods_combs.begin(); mc_it != mods_combs.end(); ++mc_it)
      {
        multi_mod_switch = false;
        OpenMS::AASequence temp_sequence = *sq_it;
        for (std::vector<size_t>::iterator pos_it = mc_it->begin(); pos_it != mc_it->end(); ++pos_it)
        {
          if (*pos_it == 0)
          {
            temp_sequence.setNTerminalModification(modification);
          }
          else if (*pos_it == temp_sequence.size() + 1)
          {
            temp_sequence.setCTerminalModification(modification);
          }
          else
          {
            if (!temp_sequence.isModified(*pos_it - 1))
            {
              temp_sequence.setModification(*pos_it - 1, modification);
            }
            else
            {
              multi_mod_switch = true;
            }
          }
        }
        if (!multi_mod_switch) { modified_sequences.push_back(temp_sequence); }
      }
    }

    return modified_sequences;
  }

  std::vector<OpenMS::AASequence> MRMAssay::combineModifications_(OpenMS::AASequence sequence)
  {
    std::map<OpenMS::String, size_t> mods;
    std::vector<OpenMS::AASequence> sequences;
    sequences.push_back(AASequence::fromString(sequence.toUnmodifiedString()));

    OpenMS::ModificationsDB* ptr = ModificationsDB::getInstance();

    if (sequence.hasNTerminalModification())
    {
      if (mods.find(sequence.getNTerminalModification()) == mods.end())
      {
        mods[sequence.getNTerminalModification()] = 0;
      }
      mods[sequence.getNTerminalModification()] += 1;
    }

    if (sequence.hasCTerminalModification())
    {
      if (mods.find(sequence.getCTerminalModification()) == mods.end())
      {
        mods[sequence.getCTerminalModification()] = 0;
      }
      mods[sequence.getCTerminalModification()] += 1;
    }

    for (size_t i = 0; i < sequence.size(); ++i)
    {
      if (sequence.isModified(i))
      {
        if (mods.find(sequence.getResidue(i).getModification()) == mods.end())
        {
          mods[sequence.getResidue(i).getModification()] = 0;
        }
        mods[sequence.getResidue(i).getModification()] += 1;
      }
    }

    for (std::map<OpenMS::String, size_t>::iterator mod_it = mods.begin(); mod_it != mods.end(); ++mod_it)
    {
      std::vector<size_t> mods_res;

      std::set<const ResidueModification*> modifiable_nterm;
      ptr->searchTerminalModifications(modifiable_nterm, mod_it->first, ResidueModification::N_TERM);
      if (!modifiable_nterm.empty())
      {
        mods_res.push_back(0);
      }

      std::set<const ResidueModification*> modifiable_cterm;
      ptr->searchTerminalModifications(modifiable_cterm, mod_it->first, ResidueModification::C_TERM);
      if (!modifiable_cterm.empty())
      {
        mods_res.push_back(sequence.size() + 1);
      }

      for (size_t i = 0; i < sequence.size(); ++i)
      {
        std::set<const ResidueModification*> modifiable_residues;
        ptr->searchModifications(modifiable_residues, sequence.getResidue(i).getOneLetterCode(), mod_it->first, ResidueModification::ANYWHERE);
        if (!modifiable_residues.empty())
        {
          mods_res.push_back(i + 1);
        }
      }
      std::vector<std::vector<size_t> > mods_combs = nchoosekcombinations_(mods_res, mod_it->second);
      sequences = addModificationsSequences_(sequences, mods_combs, mod_it->first);
    }
    return sequences;
  }

  std::vector<OpenMS::AASequence> MRMAssay::combineDecoyModifications_(OpenMS::AASequence sequence, OpenMS::AASequence decoy_sequence)
  {
    std::map<OpenMS::String, size_t> mods;
    std::vector<OpenMS::AASequence> decoy_sequences;
    decoy_sequences.push_back(AASequence::fromString(decoy_sequence.toUnmodifiedString()));

    OpenMS::ModificationsDB* ptr = ModificationsDB::getInstance();

    if (sequence.hasNTerminalModification())
    {
      mods[sequence.getNTerminalModification()] += 1;
    }

    if (sequence.hasCTerminalModification())
    {
      mods[sequence.getCTerminalModification()] += 1;
    }

    for (size_t i = 0; i < sequence.size(); ++i)
    {
      if (sequence.isModified(i))
      {
        mods[sequence.getResidue(i).getModification()] += 1;
      }
    }

    for (std::map<OpenMS::String, size_t>::iterator mod_it = mods.begin(); mod_it != mods.end(); ++mod_it)
    {
      std::vector<size_t> mods_res;

      std::set<const ResidueModification*> modifiable_nterm;
      ptr->searchTerminalModifications(modifiable_nterm, mod_it->first, ResidueModification::N_TERM);
      if (!modifiable_nterm.empty())
      {
        mods_res.push_back(0);
      }

      std::set<const ResidueModification*> modifiable_cterm;
      ptr->searchTerminalModifications(modifiable_cterm, mod_it->first, ResidueModification::C_TERM);
      if (!modifiable_cterm.empty())
      {
        mods_res.push_back(sequence.size() + 1);
      }

      for (size_t i = 0; i < sequence.size(); ++i)
      {
        std::set<const ResidueModification*> modifiable_residues;
        ptr->searchModifications(modifiable_residues, sequence.getResidue(i).getOneLetterCode(), mod_it->first, ResidueModification::ANYWHERE);
        if (!modifiable_residues.empty())
        {
          mods_res.push_back(i + 1);
        }
      }
      std::vector<std::vector<size_t> > mods_combs = nchoosekcombinations_(mods_res, mod_it->second);
      decoy_sequences = addModificationsSequences_(decoy_sequences, mods_combs, mod_it->first);
    }
    return decoy_sequences;
  }

  void MRMAssay::generateTargetInSilicoMap_(OpenMS::TargetedExperiment& exp, std::vector<String> fragment_types, std::vector<size_t> fragment_charges, bool enable_specific_losses, bool enable_unspecific_losses, std::vector<std::pair<double, double> > swathes, int round_decPow, size_t max_num_alternative_localizations, boost::unordered_map<size_t, boost::unordered_map<String, std::set<std::string> > >& TargetSequenceMap, boost::unordered_map<size_t, boost::unordered_map<String, std::vector<std::pair<double, std::string> > > >& TargetIonMap, boost::unordered_map<String, std::vector<std::pair<std::string, double> > >& TargetPeptideMap)
  {
    OpenMS::MRMIonSeries mrmis;

    // Step 1: Generate target in silico peptide map containing theoretical transitions
    Size progress = 0;
    startProgress(0, exp.getPeptides().size(), "Generation of target in silico peptide map");
    for (size_t i = 0; i < exp.getPeptides().size(); ++i)
    { 
      setProgress(progress++);

      TargetedExperiment::Peptide peptide = exp.getPeptides()[i];
      OpenMS::AASequence peptide_sequence = TargetedExperimentHelper::getAASequence(peptide);
      int precursor_swath = getSwath_(swathes, peptide_sequence.getMonoWeight(Residue::Full, peptide.getChargeState()) / peptide.getChargeState());

      // Compute all alternative peptidoforms compatible with ModificationsDB
      std::vector<OpenMS::AASequence> alternative_peptide_sequences = combineModifications_(peptide_sequence);  

      // Some permutations might be too complex, skip if threshold is reached
      if (alternative_peptide_sequences.size() > max_num_alternative_localizations)
      {
        LOG_DEBUG << "[uis] Peptide skipped (too many permutations possible): " << peptide.id << std::endl;
        continue;
      }

      // Iterate over all peptidoforms
      for (std::vector<OpenMS::AASequence>::iterator alt_aa = alternative_peptide_sequences.begin(); alt_aa != alternative_peptide_sequences.end(); ++alt_aa)
      { 
        // Append peptidoform to index
        TargetSequenceMap[precursor_swath][alt_aa->toUnmodifiedString()].insert(alt_aa->toString());
        // Generate theoretical ion series
        MRMIonSeries::IonSeries ionseries = mrmis.getIonSeries(*alt_aa, peptide.getChargeState(), fragment_types, fragment_charges, enable_specific_losses, enable_unspecific_losses);

        // Iterate over all theoretical transitions
        for (boost::unordered_map<String, double>::iterator im_it = ionseries.begin(); im_it != ionseries.end(); ++im_it)
        {
          // Append transition to indices to find interfering transitions
          TargetIonMap[precursor_swath][alt_aa->toUnmodifiedString()].push_back(std::make_pair(Math::roundDecimal(im_it->second, round_decPow), alt_aa->toString()));
          TargetPeptideMap[peptide.id].push_back(std::make_pair(im_it->first, Math::roundDecimal(im_it->second, round_decPow)));
        }
      }
    }
    endProgress();
  }

  void MRMAssay::generateDecoySequences_(boost::unordered_map<size_t, boost::unordered_map<String, std::set<std::string> > >& TargetSequenceMap, boost::unordered_map<String, String>& DecoySequenceMap, int shuffle_seed)
  {
    // Step 2a: Generate decoy sequences that share peptidoform properties with targets
    if (shuffle_seed == -1)
    {
      shuffle_seed = time(0);
    }

    boost::mt19937 generator(shuffle_seed);
    boost::uniform_int<> uni_dist;
    boost::variate_generator<boost::mt19937&, boost::uniform_int<> > pseudoRNG(generator, uni_dist);

    Size progress = 0;
    startProgress(0, TargetSequenceMap.size(), "Target-decoy mapping");
    std::string decoy_peptide_string;

    // Iterate over swathes
    for (boost::unordered_map<size_t, boost::unordered_map<String, std::set<std::string> > >::iterator sm_it = TargetSequenceMap.begin(); sm_it != TargetSequenceMap.end(); ++sm_it)
    {
      setProgress(progress++);
      // Iterate over each unmodified peptide sequence 
      for (boost::unordered_map<String, std::set<std::string> >::iterator ta_it = sm_it->second.begin(); ta_it != sm_it->second.end(); ++ta_it)
      {
        // Get a random unmodified peptide sequence as base for later modification
        if (DecoySequenceMap[ta_it->first] == "")
        {
          decoy_peptide_string = getRandomSequence_(ta_it->first.size(), pseudoRNG);
        }
        else
        {
          decoy_peptide_string = DecoySequenceMap[ta_it->first];
        }

        // Iterate over all target peptidoforms and replace decoy residues with modified target residues
        for (std::set<std::string>::iterator se_it = ta_it->second.begin(); se_it != ta_it->second.end(); ++se_it)
        {
          OpenMS::AASequence seq = AASequence::fromString(*se_it);

          if (seq.hasNTerminalModification())
          {
            decoy_peptide_string = decoy_peptide_string.replace(0, 1, seq.getSubsequence(0, 1).toUnmodifiedString());
          }

          if (seq.hasCTerminalModification())
          {
            decoy_peptide_string = decoy_peptide_string.replace(decoy_peptide_string.size() - 1, 1, seq.getSubsequence(decoy_peptide_string.size() - 1, 1).toUnmodifiedString());
          }

          for (size_t i = 0; i < seq.size(); ++i)
          {
            if (seq.isModified(i))
            {
              decoy_peptide_string = decoy_peptide_string.replace(i, 1, seq.getSubsequence(i, 1).toUnmodifiedString());
            }
          }
          DecoySequenceMap[ta_it->first] = decoy_peptide_string;
        }
      }
    }
    endProgress();
  }

  void MRMAssay::generateDecoyInSilicoMap_(OpenMS::TargetedExperiment& exp, std::vector<String> fragment_types, std::vector<size_t> fragment_charges, bool enable_specific_losses, bool enable_unspecific_losses, std::vector<std::pair<double, double> > swathes, int round_decPow, boost::unordered_map<String, TargetedExperiment::Peptide>& TargetDecoyMap, boost::unordered_map<String, std::vector<std::pair<std::string, double> > >& TargetPeptideMap, boost::unordered_map<String, String>& DecoySequenceMap, boost::unordered_map<size_t, boost::unordered_map<String, std::vector<std::pair<double, std::string> > > >& DecoyIonMap, boost::unordered_map<String, std::vector<std::pair<std::string, double> > >& DecoyPeptideMap)
  {
    MRMIonSeries mrmis;

    // Step 2b: Generate decoy in silico peptide map containing theoretical transitions
    Size progress = 0;
    startProgress(0, exp.getPeptides().size(), "Generation of decoy in silico peptide map");
    for (size_t i = 0; i < exp.getPeptides().size(); ++i)
    {
      setProgress(progress++);

      TargetedExperiment::Peptide peptide = exp.getPeptides()[i];

      // Skip if target peptide is not in map, e.g. permutation threshold was reached
      if (TargetPeptideMap.find(peptide.id) == TargetPeptideMap.end())
      {
        continue;
      }

      OpenMS::AASequence peptide_sequence = TargetedExperimentHelper::getAASequence(peptide);
      int precursor_swath = getSwath_(swathes, peptide_sequence.getMonoWeight(Residue::Full, peptide.getChargeState()) / peptide.getChargeState());

      // Copy properties of target peptide to decoy and get sequence from map
      TargetedExperiment::Peptide decoy_peptide = peptide;
      decoy_peptide.sequence = DecoySequenceMap[peptide.sequence];

      TargetDecoyMap[peptide.id] = decoy_peptide;
      OpenMS::AASequence decoy_peptide_sequence = TargetedExperimentHelper::getAASequence(decoy_peptide);

      // Compute all alternative peptidoforms compatible with ModificationsDB
      // Infers residue specificity from target sequence but is applied to decoy sequence
      std::vector<OpenMS::AASequence> alternative_decoy_peptide_sequences = combineDecoyModifications_(peptide_sequence, decoy_peptide_sequence);

      // Iterate over all peptidoforms
      for (std::vector<OpenMS::AASequence>::iterator alt_aa = alternative_decoy_peptide_sequences.begin(); alt_aa != alternative_decoy_peptide_sequences.end(); ++alt_aa)
      {
        // Generate theoretical ion series
        MRMIonSeries::IonSeries ionseries = mrmis.getIonSeries(*alt_aa, decoy_peptide.getChargeState(), fragment_types, fragment_charges, enable_specific_losses, enable_unspecific_losses);  

        // Iterate over all theoretical transitions
        for (boost::unordered_map<String, double>::iterator im_it = ionseries.begin(); im_it != ionseries.end(); ++im_it)
        {
          // Append transition to indices to find interfering transitions
          DecoyIonMap[precursor_swath][alt_aa->toUnmodifiedString()].push_back(std::make_pair(Math::roundDecimal(im_it->second, round_decPow), alt_aa->toString()));
          DecoyPeptideMap[decoy_peptide.id].push_back(std::make_pair(im_it->first, Math::roundDecimal(im_it->second, round_decPow)));
        }
      }
    }
    endProgress();
  }

  void MRMAssay::generateTargetAssays_(OpenMS::TargetedExperiment& exp, TransitionVectorType& transitions, double mz_threshold, std::vector<std::pair<double, double> > swathes, int round_decPow, boost::unordered_map<String, std::vector<std::pair<std::string, double> > >& TargetPeptideMap, boost::unordered_map<size_t, boost::unordered_map<String, std::vector<std::pair<double, std::string> > > >& TargetIonMap)
  {
    MRMIonSeries mrmis;

    // Step 3: Generate target UIS assays
    Size progress = 0;
    startProgress(0, TargetPeptideMap.size(), "Generation of target UIS assays");

    // Iterate over all target peptides
    for (boost::unordered_map<String, std::vector<std::pair<std::string, double> > >::iterator pep_it = TargetPeptideMap.begin(); pep_it != TargetPeptideMap.end(); ++pep_it)
    { 
      setProgress(progress++);

      TargetedExperiment::Peptide peptide = exp.getPeptideByRef(pep_it->first);
      OpenMS::AASequence peptide_sequence = TargetedExperimentHelper::getAASequence(peptide);
      int target_precursor_swath = getSwath_(swathes, peptide_sequence.getMonoWeight(Residue::Full, peptide.getChargeState()) / peptide.getChargeState());

      // Sort all transitions and make them unique
      std::vector<std::pair<std::string, double> > tr_vec = pep_it->second;
      std::sort(tr_vec.begin(), tr_vec.end());
      std::vector<std::pair<std::string, double> >::iterator tr_vec_end = std::unique(tr_vec.begin(), tr_vec.end());

      // Iterate over all transitions
      for (std::vector<std::pair<std::string, double> >::iterator tr_it = tr_vec.begin(); tr_it != tr_vec_end; ++tr_it)
      { 
        // Check mapping of transitions to other peptidoforms
        std::vector<std::string> isoforms = getMatchingPeptidoforms_(tr_it->second, TargetIonMap[target_precursor_swath][peptide_sequence.toUnmodifiedString()], mz_threshold);

        // Check that transition maps to at least one peptidoform
        if (isoforms.size() > 0)
        {
          ReactionMonitoringTransition trn;
          trn.setMetaValue("detecting_transition", "false");
          trn.setMetaValue("insilico_transition", "true");
          trn.setPrecursorMZ(Math::roundDecimal(peptide_sequence.getMonoWeight(Residue::Full, peptide.getChargeState()) / peptide.getChargeState(), round_decPow));
          trn.setProductMZ(tr_it->second);
          trn.setPeptideRef(peptide.id);
          mrmis.annotateTransitionCV(trn, tr_it->first);
          trn.setMetaValue("identifying_transition", "true");
          trn.setMetaValue("quantifying_transition", "false");
          // Set transition name containing mapping to peptidoforms with potential peptidoforms enumerated in brackets
          trn.setName(String("UIS") + "_{" + ListUtils::concatenate(isoforms, "|") + "}_" + String(trn.getPrecursorMZ()) + "_" + String(trn.getProductMZ()) + "_" + String(peptide.getRetentionTime()) + "_" + tr_it->first);
          trn.setNativeID(String("UIS") + "_{" + ListUtils::concatenate(isoforms, "|") + "}_" + String(trn.getPrecursorMZ()) + "_" + String(trn.getProductMZ()) + "_" + String(peptide.getRetentionTime()) + "_" + tr_it->first);

          LOG_DEBUG << "[uis] Transition " << trn.getNativeID() << std::endl;

          // Append transition
          transitions.push_back(trn);
        }
      }
      LOG_DEBUG << "[uis] Peptide " << peptide.id << std::endl;
    }
    endProgress();
  }

  void MRMAssay::generateDecoyAssays_(OpenMS::TargetedExperiment& exp, TransitionVectorType& transitions, double mz_threshold, std::vector<std::pair<double, double> > swathes, int round_decPow, boost::unordered_map<String, std::vector<std::pair<std::string, double> > >& DecoyPeptideMap, boost::unordered_map<String, TargetedExperiment::Peptide>& TargetDecoyMap, boost::unordered_map<size_t, boost::unordered_map<String, std::vector<std::pair<double, std::string> > > > DecoyIonMap, boost::unordered_map<size_t, boost::unordered_map<String, std::vector<std::pair<double, std::string> > > > TargetIonMap)
  {
    MRMIonSeries mrmis;

    // Step 4: Generate decoys UIS assays
    Size progress = 0;
    startProgress(0, DecoyPeptideMap.size(), "Generation of decoy UIS assays");

    // Iterate over all decoy peptides
    for (boost::unordered_map<String, std::vector<std::pair<std::string, double> > >::iterator decoy_pep_it = DecoyPeptideMap.begin(); decoy_pep_it != DecoyPeptideMap.end(); ++decoy_pep_it)
    {
      setProgress(progress++);
      TargetedExperiment::Peptide target_peptide = exp.getPeptideByRef(decoy_pep_it->first);
      OpenMS::AASequence target_peptide_sequence = TargetedExperimentHelper::getAASequence(target_peptide);
      int target_precursor_swath = getSwath_(swathes, target_peptide_sequence.getMonoWeight(Residue::Full, target_peptide.getChargeState()) / target_peptide.getChargeState());

      TargetedExperiment::Peptide decoy_peptide = TargetDecoyMap[decoy_pep_it->first];
      OpenMS::AASequence decoy_peptide_sequence = TargetedExperimentHelper::getAASequence(decoy_peptide);

      // Sort all transitions and make them unique
      std::vector<std::pair<std::string, double> > decoy_tr_vec = decoy_pep_it->second;
      std::sort(decoy_tr_vec.begin(), decoy_tr_vec.end());
      std::vector<std::pair<std::string, double> >::iterator decoy_tr_vec_end = std::unique(decoy_tr_vec.begin(), decoy_tr_vec.end());

      // Iterate over all transitions
      for (std::vector<std::pair<std::string, double> >::iterator decoy_tr_it = decoy_tr_vec.begin(); decoy_tr_it != decoy_tr_vec_end; ++decoy_tr_it)
      {
        // Check mapping of transitions to other peptidoforms
        std::vector<std::string> decoy_isoforms = getMatchingPeptidoforms_(decoy_tr_it->second, DecoyIonMap[target_precursor_swath][decoy_peptide_sequence.toUnmodifiedString()], mz_threshold);

        // Check that transition maps to at least one peptidoform
        if (decoy_isoforms.size() > 0)
        {
          ReactionMonitoringTransition trn;
          trn.setDecoyTransitionType(ReactionMonitoringTransition::DECOY);
          trn.setMetaValue("detecting_transition", "false");
          trn.setMetaValue("insilico_transition", "true");
          trn.setPrecursorMZ(Math::roundDecimal(target_peptide_sequence.getMonoWeight(Residue::Full, target_peptide.getChargeState()) / target_peptide.getChargeState(), round_decPow));
          trn.setProductMZ(decoy_tr_it->second);
          trn.setPeptideRef(decoy_peptide.id);
          mrmis.annotateTransitionCV(trn, decoy_tr_it->first);
          trn.setMetaValue("identifying_transition", "true");
          trn.setMetaValue("quantifying_transition", "false");
          // Set transition name containing mapping to peptidoforms with potential peptidoforms enumerated in brackets
          trn.setName(String("UISDECOY") + "_{" + ListUtils::concatenate(decoy_isoforms, "|") + "}_" + String(trn.getPrecursorMZ()) + "_" + String(trn.getProductMZ()) + "_" + String(decoy_peptide.getRetentionTime()) + "_" + decoy_tr_it->first);
          trn.setNativeID(String("UISDECOY") + "_{" + ListUtils::concatenate(decoy_isoforms, "|") + "}_" + String(trn.getPrecursorMZ()) + "_" + String(trn.getProductMZ()) + "_" + String(decoy_peptide.getRetentionTime()) + "_" + decoy_tr_it->first);

          LOG_DEBUG << "[uis] Decoy transition " << trn.getNativeID() << std::endl;


          // Check if decoy transition is overlapping with target transition
          std::vector<std::string> target_isoforms_overlap = getMatchingPeptidoforms_(decoy_tr_it->second, TargetIonMap[target_precursor_swath][target_peptide_sequence.toUnmodifiedString()], mz_threshold);

          if (target_isoforms_overlap.size() > 0)
          {
            LOG_DEBUG << "[uis] Skipping overlapping decoy transition " << trn.getNativeID() << std::endl;
            continue;
          }
          else
          {
            // Append transition
            transitions.push_back(trn);
          }
        }
      }
    }
    endProgress();
  }

  void MRMAssay::reannotateTransitions(OpenMS::TargetedExperiment& exp, double precursor_mz_threshold, double product_mz_threshold, std::vector<String> fragment_types, std::vector<size_t> fragment_charges, bool enable_reannotation, bool enable_specific_losses, bool enable_unspecific_losses, int round_decPow)
  {
    PeptideVectorType peptides;
    ProteinVectorType proteins;
    TransitionVectorType transitions;

    OpenMS::MRMIonSeries mrmis;

    Size progress = 0;
    startProgress(0, exp.getTransitions().size(), "Annotating transitions");
    size_t j = 0; // transition index
    for (size_t i = 0; i < exp.getTransitions().size(); ++i)
    {
      setProgress(++progress);
      ReactionMonitoringTransition tr = exp.getTransitions()[i];

      TargetedExperiment::Peptide target_peptide = exp.getPeptideByRef(tr.getPeptideRef());
      OpenMS::AASequence target_peptide_sequence = TargetedExperimentHelper::getAASequence(target_peptide);

      // Generate new ID (transition_group_id) for target peptide
      target_peptide.id = String(target_peptide.protein_refs[0]) + String("_") + TargetedExperimentHelper::getAASequence(target_peptide).toString() + String("_") + String(target_peptide.getChargeState()) + "_" + target_peptide.rts[0].getCVTerms()["MS:1000896"][0].getValue().toString();

      // Annotate transition: Either set correct CV terms from annotation or (if enable_reannotation == true) do annotation using theoretical ion series
      // Parameters set allowed fragment charges, tolerance, etc. All unannoted transitions are discarded
      mrmis.annotateTransition(tr, target_peptide, precursor_mz_threshold, product_mz_threshold, enable_reannotation, fragment_types, fragment_charges, enable_specific_losses, enable_unspecific_losses, round_decPow);

      // Skip unannotated transitions from previous step
      if (tr.getProduct().getInterpretationList()[0].hasCVTerm("MS:1001240"))
      {
        LOG_DEBUG << "[unannotated] Skipping " << target_peptide_sequence.toString() << " PrecursorMZ: " << tr.getPrecursorMZ() << " ProductMZ: " << tr.getProductMZ() << " " << tr.getMetaValue("annotation") << std::endl;
        continue;
      }
      else
      {
        LOG_DEBUG << "[selected] " << target_peptide_sequence.toString() << " PrecursorMZ: " << tr.getPrecursorMZ() << " ProductMZ: " << tr.getProductMZ() << " " << tr.getMetaValue("annotation") << std::endl;
      }

      // Add reference to parent precursor
      tr.setPeptideRef(target_peptide.id);

      // Generate new ID (transition_name) for target transition
      tr.setNativeID(String(j) + String("_") +  String(target_peptide.protein_refs[0]) + String("_") + target_peptide.sequence + String("_") + String(tr.getPrecursorMZ()) + "_" + String(tr.getProductMZ()));
      j += 1; // increment transition index

      // Append transition
      transitions.push_back(tr);

      // Append precursor / peptide
      if (std::find(peptides.begin(), peptides.end(), target_peptide) == peptides.end())
      {
        peptides.push_back(target_peptide);
        LOG_DEBUG << "[selected] " <<  target_peptide_sequence.toString() << std::endl;
      }
    }
    endProgress();

    exp.setTransitions(transitions);
    exp.setPeptides(peptides);
  }

  void MRMAssay::restrictTransitions(OpenMS::TargetedExperiment& exp, double lower_mz_limit, double upper_mz_limit, std::vector<std::pair<double, double> > swathes)
  {
    OpenMS::MRMIonSeries mrmis;
    PeptideVectorType peptides;
    ProteinVectorType proteins;
    TransitionVectorType transitions;

    Size progress = 0;
    startProgress(0, exp.getTransitions().size(), "Restricting transitions");
    for (Size i = 0; i < exp.getTransitions().size(); ++i)
    {
      setProgress(++progress);
      ReactionMonitoringTransition tr = exp.getTransitions()[i];

      const TargetedExperiment::Peptide target_peptide = exp.getPeptideByRef(tr.getPeptideRef());
      OpenMS::AASequence target_peptide_sequence = TargetedExperimentHelper::getAASequence(target_peptide);

      // Check annotation for unannotated interpretations
      if (tr.getProduct().getInterpretationList().size() > 0)
      {
        // Check if transition is unannotated at primary annotation and if yes, skip
        if (tr.getProduct().getInterpretationList()[0].hasCVTerm("MS:1001240"))
        {
          LOG_DEBUG << "[unannotated] Skipping " << target_peptide_sequence << " PrecursorMZ: " << tr.getPrecursorMZ() << " ProductMZ: " << tr.getProductMZ() << " " << tr.getMetaValue("annotation") << std::endl;
          continue;
        }
      }

      // Check if product m/z falls into swath from precursor m/z and if yes, skip
      if (swathes.size() > 0)
      {
        if (MRMAssay::isInSwath_(swathes, tr.getPrecursorMZ(), tr.getProductMZ()))
        {
          LOG_DEBUG << "[swath] Skipping " << target_peptide_sequence << " PrecursorMZ: " << tr.getPrecursorMZ() << " ProductMZ: " << tr.getProductMZ() << std::endl;
          continue;
        }
      }

      // Check if product m/z is outside of m/z boundaries and if yes, skip
      if (tr.getProductMZ() < lower_mz_limit || tr.getProductMZ() > upper_mz_limit)
      {
        LOG_DEBUG << "[mz_limit] Skipping " << target_peptide_sequence << " PrecursorMZ: " << tr.getPrecursorMZ() << " ProductMZ: " << tr.getProductMZ() << std::endl;
        continue;
      }

      // Append transition
      transitions.push_back(tr);
    }
    endProgress();

    exp.setTransitions(transitions);
  }

  void MRMAssay::detectingTransitions(OpenMS::TargetedExperiment& exp, int min_transitions, int max_transitions)
  {
    PeptideVectorType peptides;
    std::vector<String> peptide_ids;
    ProteinVectorType proteins;
    TransitionVectorType transitions;

    Map<String, TransitionVectorType> TransitionsMap;

    // Generate a map of peptides to transitions for easy access
    for (Size i = 0; i < exp.getTransitions().size(); ++i)
    {
      ReactionMonitoringTransition tr = exp.getTransitions()[i];

      if (TransitionsMap.find(tr.getPeptideRef()) == TransitionsMap.end())
      {
        TransitionsMap[tr.getPeptideRef()];
      }

      TransitionsMap[tr.getPeptideRef()].push_back(tr);
    }

    for (Map<String, TransitionVectorType>::iterator m = TransitionsMap.begin();
         m != TransitionsMap.end(); ++m)
    {
      // Ensure that all precursors have the minimum number of transitions
      if (m->second.size() >= (Size)min_transitions)
      {
        // LibraryIntensity stores all reference transition intensities of a precursor
        std::vector<double> LibraryIntensity;
        for (TransitionVectorType::iterator tr_it = m->second.begin(); tr_it != m->second.end(); ++tr_it)
        {
          LibraryIntensity.push_back(boost::lexical_cast<double>(tr_it->getLibraryIntensity()));
        }

        // Sort by intensity, reverse and delete all elements after max_transitions to find the best candidates
        std::sort(LibraryIntensity.begin(), LibraryIntensity.end());
        std::reverse(LibraryIntensity.begin(), LibraryIntensity.end());
        if ((Size)max_transitions < LibraryIntensity.size())
        {
          std::vector<double>::iterator start_delete = LibraryIntensity.begin();
          std::advance(start_delete, max_transitions);
          LibraryIntensity.erase(start_delete, LibraryIntensity.end());
        }

        // Check if transitions are among the ones with maximum intensity
        // If several transitions have the same intensities ensure restriction max_transitions
        Size j = 0; // transition number index
        for (TransitionVectorType::iterator tr_it = m->second.begin(); tr_it != m->second.end(); ++tr_it)
        {
          ReactionMonitoringTransition tr = *tr_it;

          if ((std::find(LibraryIntensity.begin(), LibraryIntensity.end(), boost::lexical_cast<double>(tr.getLibraryIntensity())) != LibraryIntensity.end()) && tr.getDecoyTransitionType() != ReactionMonitoringTransition::DECOY && j < (Size)max_transitions)
          {
            // Set meta value tag for detecting transition
            tr.setMetaValue("detecting_transition", "true");
            j += 1;
          }
          else
          {
            continue;
          }

          // Append transition
          transitions.push_back(tr);

          // Append transition_group_id to index
          if (std::find(peptide_ids.begin(), peptide_ids.end(), tr.getPeptideRef()) == peptide_ids.end())
          {
            peptide_ids.push_back(tr.getPeptideRef());
          }
        }
      }
    }

    std::vector<String> ProteinList;
    for (Size i = 0; i < exp.getPeptides().size(); ++i)
    {
      TargetedExperiment::Peptide peptide = exp.getPeptides()[i];

      // Check if peptide has any transitions left
      if (std::find(peptide_ids.begin(), peptide_ids.end(), peptide.id) != peptide_ids.end())
      {
        peptides.push_back(peptide);
        for (Size j = 0; j < peptide.protein_refs.size(); ++j)
        {
          ProteinList.push_back(peptide.protein_refs[j]);
        }
      }
      else
      {
        LOG_DEBUG << "[peptide] Skipping " << peptide.id << std::endl;
      }
    }

    for (Size i = 0; i < exp.getProteins().size(); ++i)
    {
      OpenMS::TargetedExperiment::Protein protein = exp.getProteins()[i];

      // Check if protein has any peptides left
      if (find(ProteinList.begin(), ProteinList.end(), protein.id) != ProteinList.end())
      {
        proteins.push_back(protein);
      }
      else
      {
        LOG_DEBUG << "[protein] Skipping " << protein.id << std::endl;
      }
    }

    exp.setTransitions(transitions);
    exp.setPeptides(peptides);
    exp.setProteins(proteins);
  }

  void MRMAssay::uisTransitions(OpenMS::TargetedExperiment& exp, std::vector<String> fragment_types, std::vector<size_t> fragment_charges, bool enable_specific_losses, bool enable_unspecific_losses, double mz_threshold, std::vector<std::pair<double, double> > swathes, int round_decPow, size_t max_num_alternative_localizations, int shuffle_seed)
  {
    OpenMS::MRMIonSeries mrmis;

    TransitionVectorType transitions = exp.getTransitions();

    // Different maps to store temporary data for fast access
    // TargetIonMap & DecoyIonMap: Store product m/z of all peptidoforms to find interfering transitions
    boost::unordered_map<size_t, boost::unordered_map<String, std::vector<std::pair<double, std::string> > > > TargetIonMap, DecoyIonMap;
    // TargetPeptideMap & DecoyPeptideMap: Store theoretical transitons of all peptidoforms
    boost::unordered_map<String, std::vector<std::pair<std::string, double> > > TargetPeptideMap, DecoyPeptideMap;
    // TargetSequenceMap, DecoySequenceMap & TargetDecoyMap: Link targets and UIS decoys
    boost::unordered_map<size_t, boost::unordered_map<String, std::set<std::string> > > TargetSequenceMap;
    boost::unordered_map<String, String> DecoySequenceMap;
    boost::unordered_map<String, TargetedExperiment::Peptide> TargetDecoyMap;

    // Step 1: Generate target in silico peptide map containing theoretical transitions
    generateTargetInSilicoMap_(exp, fragment_types, fragment_charges, enable_specific_losses, enable_unspecific_losses, swathes, round_decPow, max_num_alternative_localizations, TargetSequenceMap, TargetIonMap, TargetPeptideMap);

    // Step 2a: Generate decoy sequences that share peptidoform properties with targets
    generateDecoySequences_(TargetSequenceMap, DecoySequenceMap, shuffle_seed);

    // Step 2b: Generate decoy in silico peptide map containing theoretical transitions
    generateDecoyInSilicoMap_(exp, fragment_types, fragment_charges, enable_specific_losses, enable_unspecific_losses, swathes, round_decPow, TargetDecoyMap, TargetPeptideMap, DecoySequenceMap, DecoyIonMap, DecoyPeptideMap);

    // Step 3: Generate target UIS assays
    generateTargetAssays_(exp, transitions, mz_threshold, swathes, round_decPow, TargetPeptideMap, TargetIonMap);

    // Step 4: Generate decoys UIS assays
    generateDecoyAssays_(exp, transitions, mz_threshold, swathes, round_decPow, DecoyPeptideMap, TargetDecoyMap, DecoyIonMap, TargetIonMap);

    exp.setTransitions(transitions);
  }

}
