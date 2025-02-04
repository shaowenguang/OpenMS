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
// $Maintainer: Mathias Walzer $
// $Authors: Nico Pfeifer, Mathias Walzer, Hendrik Weisser $
// --------------------------------------------------------------------------

#include <OpenMS/FILTERING/ID/IDFilter.h>

#include <OpenMS/CHEMISTRY/ModificationsDB.h>
#include <OpenMS/CONCEPT/LogStream.h>

#include <cmath>
#include <climits>

using namespace std;

namespace OpenMS
{
  IDFilter::IDFilter()
  {
  }

  IDFilter::~IDFilter()
  {
  }


  struct IDFilter::HasMinPeptideLength
  {
    typedef PeptideHit argument_type; // for use as a predicate

    Size length;

    explicit HasMinPeptideLength(Size length):
      length(length)
    {}
    
    bool operator()(const PeptideHit& hit) const
    {
      return hit.getSequence().size() >= length;
    }
  };


  struct IDFilter::HasMinCharge
  {
    typedef PeptideHit argument_type; // for use as a predicate

    Int charge;

    explicit HasMinCharge(Int charge):
      charge(charge)
    {}

    bool operator()(const PeptideHit& hit) const
    {
      return hit.getCharge() >= charge;
    }
  };


  struct IDFilter::HasLowMZError
  {
    typedef PeptideHit argument_type; // for use as a predicate

    double precursor_mz, tolerance;

    HasLowMZError(double precursor_mz, double tolerance, bool unit_ppm):
      precursor_mz(precursor_mz), tolerance(tolerance)
    {
      if (unit_ppm) this->tolerance *= precursor_mz / 1.0e6;
    }

    bool operator()(const PeptideHit& hit) const
    {
      Int z = hit.getCharge();
      if (z == 0) z = 1;
      double peptide_mz = (hit.getSequence().getMonoWeight(Residue::Full, z) /
                           double(z));
      return fabs(precursor_mz - peptide_mz) <= tolerance;
    }
  };


  struct IDFilter::HasMatchingModification
  {
    typedef PeptideHit argument_type; // for use as a predicate

    const set<String>& mods;

    explicit HasMatchingModification(const set<String>& mods):
      mods(mods)
    {}

    bool operator()(const PeptideHit& hit) const
    {
      const AASequence& seq = hit.getSequence();
      if (mods.empty()) return seq.isModified();

      for (Size i = 0; i < seq.size(); ++i)
      {
        if (seq.isModified(i))
        {
          String mod_name = seq[i].getModification() + " (" +
            seq[i].getOneLetterCode() + ")";
          if (mods.count(mod_name) > 0) return true;
        }
      }

      // terminal modifications:
      if (seq.hasNTerminalModification())
      {
        String mod_name = seq.getNTerminalModification() + " (N-term)";
        if (mods.count(mod_name) > 0) return true;
        // amino acid-specific terminal mod. (e.g. "Ammonia-loss (N-term C"):
        mod_name[mod_name.size() - 1] = ' ';
        mod_name += seq[Size(0)].getOneLetterCode() + ")";
        if (mods.count(mod_name) > 0) return true;
      }
      if (seq.hasCTerminalModification())
      {
        String mod_name = seq.getCTerminalModification() + " (C-term)";
        if (mods.count(mod_name) > 0) return true;
        // amino acid-specific terminal mod. (e.g. "Arg-loss (C-term R)"):
        mod_name[mod_name.size() - 1] = ' ';
        mod_name += seq[seq.size() - 1].getOneLetterCode() + ")";
        if (mods.count(mod_name) > 0) return true;
      }

      return false;
    }
  };


  struct IDFilter::HasMatchingSequence
  {
    typedef PeptideHit argument_type; // for use as a predicate

    const set<String>& sequences;
    bool ignore_mods;

    HasMatchingSequence(const set<String>& sequences, bool ignore_mods = false):
      sequences(sequences), ignore_mods(ignore_mods)
    {}

    bool operator()(const PeptideHit& hit) const
    {
      const String& query = (ignore_mods ? 
                             hit.getSequence().toUnmodifiedString() :
                             hit.getSequence().toString());
      return (sequences.count(query) > 0);
    }
  };


  struct IDFilter::HasNoEvidence
  {
    typedef PeptideHit argument_type; // for use as a predicate

    bool operator()(const PeptideHit& hit) const
    {
      return hit.getPeptideEvidences().empty();
    }
  };


  struct IDFilter::HasRTInRange
  {
    typedef PeptideIdentification argument_type; // for use as a predicate

    double rt_min, rt_max;

    HasRTInRange(double rt_min, double rt_max):
      rt_min(rt_min), rt_max(rt_max)
    {}

    bool operator()(const PeptideIdentification& id) const
    {
      double rt = id.getRT();
      return (rt >= rt_min) && (rt <= rt_max);
    }
  };


  struct IDFilter::HasMZInRange
  {
    typedef PeptideIdentification argument_type; // for use as a predicate

    double mz_min, mz_max;

    HasMZInRange(double mz_min, double mz_max):
      mz_min(mz_min), mz_max(mz_max)
    {}

    bool operator()(const PeptideIdentification& id) const
    {
      double mz = id.getMZ();
      return (mz >= mz_min) && (mz <= mz_max);
    }
  };


  void IDFilter::extractPeptideSequences(
    const vector<PeptideIdentification>& peptides, set<String>& sequences,
    bool ignore_mods)
  {
    for (vector<PeptideIdentification>::const_iterator pep_it =
           peptides.begin(); pep_it != peptides.end(); ++pep_it)
    {
      for (vector<PeptideHit>::const_iterator hit_it = 
             pep_it->getHits().begin(); hit_it != pep_it->getHits().end(); 
           ++hit_it)
      {
        if (ignore_mods)
        {
          sequences.insert(hit_it->getSequence().toUnmodifiedString());
        }
        else
        {
          sequences.insert(hit_it->getSequence().toString());
        }
      }
    }
  }


  void IDFilter::removeUnreferencedProteins(
    vector<ProteinIdentification>& proteins,
    const vector<PeptideIdentification>& peptides)
  {
    // collect accessions that are referenced by peptides for each ID run:
    map<String, set<String> > run_to_accessions;
    for (vector<PeptideIdentification>::const_iterator pep_it = 
           peptides.begin(); pep_it != peptides.end(); ++pep_it)
    {
      const String& run_id = pep_it->getIdentifier();
      // extract protein accessions of each peptide hit:
      for (vector<PeptideHit>::const_iterator hit_it = 
             pep_it->getHits().begin(); hit_it != pep_it->getHits().end();
           ++hit_it)
      {
        const set<String>& current_accessions = 
          hit_it->extractProteinAccessions();
        run_to_accessions[run_id].insert(current_accessions.begin(),
                                         current_accessions.end());
      }
    }

    for (vector<ProteinIdentification>::iterator prot_it = proteins.begin();
         prot_it != proteins.end(); ++prot_it)
    {
      const String& run_id = prot_it->getIdentifier();
      const set<String>& accessions = run_to_accessions[run_id];
      struct HasMatchingAccession<ProteinHit> acc_filter(accessions);
      keepMatchingItems(prot_it->getHits(), acc_filter);
    }
  }


  void IDFilter::updateProteinReferences(
    vector<PeptideIdentification>& peptides,
    const vector<ProteinIdentification>& proteins,
    bool remove_peptides_without_reference)
  {
    // collect valid protein accessions for each ID run:
    map<String, set<String> > run_to_accessions;
    for (vector<ProteinIdentification>::const_iterator prot_it = 
           proteins.begin(); prot_it != proteins.end(); ++prot_it)
    {
      const String& run_id = prot_it->getIdentifier();
      for (vector<ProteinHit>::const_iterator hit_it = 
             prot_it->getHits().begin(); hit_it != prot_it->getHits().end();
           ++hit_it)
      {
        run_to_accessions[run_id].insert(hit_it->getAccession());
      }
    }

    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      const String& run_id = pep_it->getIdentifier();
      const set<String>& accessions = run_to_accessions[run_id];
      struct HasMatchingAccession<PeptideEvidence> acc_filter(accessions);
      // check protein accessions of each peptide hit
      for (vector<PeptideHit>::iterator hit_it = pep_it->getHits().begin();
           hit_it != pep_it->getHits().end(); ++hit_it)
      {
        // no non-const "PeptideHit::getPeptideEvidences" implemented, so we
        // can't use "keepMatchingItems":
        vector<PeptideEvidence> evidences;
        remove_copy_if(hit_it->getPeptideEvidences().begin(),
                       hit_it->getPeptideEvidences().end(),
                       back_inserter(evidences),
                       not1(acc_filter));
        hit_it->setPeptideEvidences(evidences);
      }

      if (remove_peptides_without_reference)
      {
        removeMatchingItems(pep_it->getHits(), HasNoEvidence());
      }
    }
  }


  bool IDFilter::updateProteinGroups(
    vector<ProteinIdentification::ProteinGroup>& groups, 
    const vector<ProteinHit>& hits)
  {
    if (groups.empty()) return true; // nothing to update

    // we'll do lots of look-ups, so use a suitable data structure:
    set<String> valid_accessions;
    for (vector<ProteinHit>::const_iterator hit_it = hits.begin();
         hit_it != hits.end(); ++hit_it)
    {
      valid_accessions.insert(hit_it->getAccession());
    }

    bool valid = true;
    vector<ProteinIdentification::ProteinGroup> filtered_groups;
    for (vector<ProteinIdentification::ProteinGroup>::iterator group_it =
           groups.begin(); group_it != groups.end(); ++group_it)
    {
      ProteinIdentification::ProteinGroup filtered;
      set_intersection(group_it->accessions.begin(), group_it->accessions.end(),
                       valid_accessions.begin(), valid_accessions.end(),
                       inserter(filtered.accessions,
                                filtered.accessions.begin()));
      if (!filtered.accessions.empty())
      {
        if (filtered.accessions.size() < group_it->accessions.size())
        {
          valid = false; // some proteins removed from group
        }
        filtered.probability = group_it->probability;
        filtered_groups.push_back(filtered);
      }
    }
    groups.swap(filtered_groups);

    return valid;
  }


  void IDFilter::keepBestPeptideHits(vector<PeptideIdentification>& peptides,
                                     bool strict)
  {
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      vector<PeptideHit>& hits = pep_it->getHits();
      if (hits.size() > 1)
      {
        pep_it->sort();
        double top_score = hits[0].getScore();
        bool higher_better = pep_it->isHigherScoreBetter();
        struct HasGoodScore<PeptideHit> good_score(top_score, higher_better);
        if (strict) // only one best score allowed
        {
          if (good_score(hits[1])) // two (or more) best-scoring hits
          {
            hits.clear();
          }
          else
          {
            hits.resize(1);
          }
        }
        else
        {
          // we could use keepMatchingHits() here, but it would be less
          // efficient (since the hits are already sorted by score):
          for (vector<PeptideHit>::iterator hit_it = ++hits.begin();
               hit_it != hits.end(); ++hit_it)
          {
            if (!good_score(*hit_it))
            {
              hits.erase(hit_it, hits.end());
              break;
            }
          }
        }
      }
    }
  }


  void IDFilter::filterPeptidesByLength(vector<PeptideIdentification>& peptides,
                                        Size min_length, Size max_length)
  {
    if (min_length > 0)
    {
      struct HasMinPeptideLength length_filter(min_length);
      for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
           pep_it != peptides.end(); ++pep_it)
      {
        keepMatchingItems(pep_it->getHits(), length_filter);
      }
    }
    ++max_length; // the predicate tests for ">=", we need ">"
    if (max_length > min_length)
    {
      struct HasMinPeptideLength length_filter(max_length);
      for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
           pep_it != peptides.end(); ++pep_it)
      {
        removeMatchingItems(pep_it->getHits(), length_filter);
      }
    }
  }


  void IDFilter::filterPeptidesByCharge(vector<PeptideIdentification>& peptides,
                                        Int min_charge, Int max_charge)
  {
    struct HasMinCharge charge_filter(min_charge);
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      keepMatchingItems(pep_it->getHits(), charge_filter);
    }
    ++max_charge; // the predicate tests for ">=", we need ">"
    if (max_charge > min_charge)
    {
      charge_filter = HasMinCharge(max_charge);
      for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
           pep_it != peptides.end(); ++pep_it)
      {
        removeMatchingItems(pep_it->getHits(), charge_filter);
      }
    }
  }


  void IDFilter::filterPeptidesByRT(vector<PeptideIdentification>& peptides,
                                    double min_rt, double max_rt)
  {
    struct HasRTInRange rt_filter(min_rt, max_rt);
    keepMatchingItems(peptides, rt_filter);
  }


  void IDFilter::filterPeptidesByMZ(vector<PeptideIdentification>& peptides,
                                    double min_mz, double max_mz)
  {
    struct HasMZInRange mz_filter(min_mz, max_mz);
    keepMatchingItems(peptides, mz_filter);
  }


  void IDFilter::filterPeptidesByMZError(
    vector<PeptideIdentification>& peptides, double mass_error, bool unit_ppm)
  {
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      struct HasLowMZError error_filter(pep_it->getMZ(), mass_error, unit_ppm);
      keepMatchingItems(pep_it->getHits(), error_filter);
    }
  }


  void IDFilter::filterPeptidesByRTPredictPValue(
    vector<PeptideIdentification>& peptides, const String& metavalue_key,
    double threshold)
  {
    Size n_initial = 0, n_metavalue = 0; // keep track of numbers of hits
    struct HasMetaValue<PeptideHit> present_filter(metavalue_key, DataValue());
    double cutoff = 1 - threshold; // why? - Hendrik
    struct HasMaxMetaValue<PeptideHit> pvalue_filter(metavalue_key, cutoff);
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      n_initial += pep_it->getHits().size();
      keepMatchingItems(pep_it->getHits(), present_filter);
      n_metavalue += pep_it->getHits().size();
    
      keepMatchingItems(pep_it->getHits(), pvalue_filter);
    }

    if (n_metavalue < n_initial)
    {
      LOG_WARN << "Filtering peptides by RTPredict p-value removed "
               << (n_initial - n_metavalue) << " of " << n_initial
               << " hits (total) that were missing the required meta value ('"
               << metavalue_key << "', added by RTPredict)." << endl;
    }
  }


  void IDFilter::removePeptidesWithMatchingModifications(
    vector<PeptideIdentification>& peptides, 
    const set<String>& modifications)
  {
    struct HasMatchingModification mod_filter(modifications);
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      removeMatchingItems(pep_it->getHits(), mod_filter);
    }
  }


  void IDFilter::keepPeptidesWithMatchingModifications(
    vector<PeptideIdentification>& peptides, 
    const set<String>& modifications)
  {
    struct HasMatchingModification mod_filter(modifications);
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      keepMatchingItems(pep_it->getHits(), mod_filter);
    }
  }


  void IDFilter::removePeptidesWithMatchingSequences(
    vector<PeptideIdentification>& peptides,
    const vector<PeptideIdentification>& bad_peptides, bool ignore_mods)
  {
    set<String> bad_seqs;
    extractPeptideSequences(bad_peptides, bad_seqs, ignore_mods);
    struct HasMatchingSequence seq_filter(bad_seqs, ignore_mods);
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      removeMatchingItems(pep_it->getHits(), seq_filter);
    }
  }


  void IDFilter::keepPeptidesWithMatchingSequences(
    vector<PeptideIdentification>& peptides,
    const vector<PeptideIdentification>& good_peptides, bool ignore_mods)
  {
    set<String> good_seqs;
    extractPeptideSequences(good_peptides, good_seqs, ignore_mods);
    struct HasMatchingSequence seq_filter(good_seqs, ignore_mods);
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      keepMatchingItems(pep_it->getHits(), seq_filter);
    }
  }


  void IDFilter::keepUniquePeptidesPerProtein(vector<PeptideIdentification>&
                                              peptides)
  {
    Size n_initial = 0, n_metavalue = 0; // keep track of numbers of hits
    struct HasMetaValue<PeptideHit> present_filter("protein_references",
                                                   DataValue());
    struct HasMetaValue<PeptideHit> unique_filter("protein_references", 
                                                  DataValue("unique"));
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      n_initial += pep_it->getHits().size();
      keepMatchingItems(pep_it->getHits(), present_filter);
      n_metavalue += pep_it->getHits().size();
    
      keepMatchingItems(pep_it->getHits(), unique_filter);
    }

    if (n_metavalue < n_initial)
    {
      LOG_WARN << "Filtering peptides by unique match to a protein removed "
               << (n_initial - n_metavalue) << " of " << n_initial
               << " hits (total) that were missing the required meta value "
               << "('protein_references', added by PeptideIndexer)." << endl;
    }
  }


  // @TODO: generalize this to protein hits?
  void IDFilter::removeDuplicatePeptideHits(vector<PeptideIdentification>&
                                            peptides)
  {
    for (vector<PeptideIdentification>::iterator pep_it = peptides.begin();
         pep_it != peptides.end(); ++pep_it)
    {
      // there's no "PeptideHit::operator<" defined, so we can't use a set nor
      // "sort" + "unique" from the standard library:
      vector<PeptideHit> filtered_hits;
      for (vector<PeptideHit>::iterator hit_it = pep_it->getHits().begin();
           hit_it != pep_it->getHits().end(); ++hit_it)
      {
        if (find(filtered_hits.begin(), filtered_hits.end(), *hit_it) == 
            filtered_hits.end())
        {
          filtered_hits.push_back(*hit_it);
        }
      }
      pep_it->getHits().swap(filtered_hits);
    }
  }

} // namespace OpenMS
