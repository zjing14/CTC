#include "ctc_plugin.h"
#include "ctc_workflow.h"
#include "ctc_engine.h"
#include "ctc_util.h"
#include <iostream>
#include <stdlib.h>
using namespace std;

// A simple example workflow that maps paired-end reads with BWA
// and converts the output to the BAM format.

int main(int argc, char **argv)
{

    if(argc != 2) {
        cerr << "Usage: " << argv[0] << " engine_dir " << endl;
        exit(-1);
    }

    string engine_dir = argv[1];

    try {
        Workflow workflow;

        // Construct inputs of the workflow
        Parameter p1(DATA_FILE, "", "fastq", "");
        workflow.addInput("in_read1", p1);
        Parameter p2(DATA_FILE, "", "fastq", "");
        workflow.addInput("in_read2", p2);
        Parameter p3(DATA_FILE, "", "bwa_index", "");
        workflow.addInput("in_genome", p3);

        // Construct steps of the workflow
        WorkflowStep s_aln1(PLUGIN, "aln1", "bwa_aln");
        s_aln1.addInput("input_read", "$in_read1");
        s_aln1.addInput("ref_genome", "$in_genome");
        s_aln1.addOutput("output_sai");
        workflow.addStep(s_aln1);

        WorkflowStep s_aln2(PLUGIN, "aln2", "bwa_aln");
        s_aln2.addInput("input_read", "$in_read2");
        s_aln2.addInput("ref_genome", "$in_genome");
        s_aln2.addOutput("output_sai");
        workflow.addStep(s_aln2);

        WorkflowStep s_tosam(PLUGIN, "tosam", "bwa_sampe");
        s_tosam.addInput("input_read1", "$in_read1");
        s_tosam.addInput("input_read2", "$in_read2");
        s_tosam.addInput("ref_genome", "$in_genome");
        s_tosam.addInput("input_sai1", "$aln1.output_sai");
        s_tosam.addInput("input_sai2", "$aln2.output_sai");
        s_tosam.addOutput("output_sam");
        workflow.addStep(s_tosam);

        WorkflowStep s_tobam(PLUGIN, "tobam", "sam_sorted_bam");
        s_tobam.addInput("input_sam", "$tosam.output_sam");
        s_tobam.addOutput("output_bam");
        workflow.addStep(s_tobam);

        // Construct outputs of the workflow
        Parameter p4(DATA_FILE, "$tobam.output_bam", "bam", "");
        workflow.addOutput("output", p4);

        Engine engine(engine_dir);

        // Configure incoming parameters
        map <string, string> paras;
        paras["in_read1"] = "/path/to/in1.fast1";
        paras["in_read2"] = "/path/to/in1.fast2";
        paras["in_genome"] = "/path/to/genome.fasta";
        paras["output"] = "/path/to/results.bam";

        engine.executeWorkflow(workflow, paras, true);

    } catch(const char *errmsg) {
        cerr << errmsg << endl;
        return -1;
    }

    return 0;
}
