#!/usr/bin/env python
import sys, optparse, os.path, glob
INSTALL_PATH="/ctc-shared/engine-test/executable/dindel/"
def main():
        parser = optparse.OptionParser()
        parser.add_option("-b", "--bamfile",
                help="Sets path to bamfile", type="string")
        parser.add_option("-r", "--reffile",
                help="Sets path to reference file", type="string")
        parser.add_option("--doDiploid",
                help="all input BAM .les are from a single diploid individual", action="store_true", default=False)
        parser.add_option("--doPooled",
                help="the number of haplotypes is unlimited", action="store_true", default=False)
        parser.add_option("-m", "--maxRead",
                help="limits the number of reads", type="int")
        parser.add_option("-f", "--filterHaplotypes",
                help="applies filtering to the set of candidate haplotypes", action="store_true", default=False)
        parser.add_option("-a", "--maxHap",
                help="number of candidate haplotypes", type="int")
        parser.add_option("-q", "--quiet",
                help="number of candidate haplotypes", action="store_true", default=False)
        parser.add_option("-o", "--output",
                help="file-prefix for output VCF file", type="string")
        parser.add_option("-i", "--bamindex",
                help="Sets path to bam index file",  type='string')
        (options, args) = parser.parse_args()

        if options.bamfile == None or options.reffile == None or options.output == None: 
            print "Usage: -b bamfile -r reffile --o output --doDiploid | --doPooled [--maxRead | --filerHaplotypes | --maxHap]" 
            sys.exit(0)

        if options.doDiploid == False and options.doPooled == False:
            print "Usage: -b bamfile -r reffile --o output --doDiploid | --doPooled [--maxRead | --filerHaplotypes | --maxHap]" 
            sys.exit(0)
        elif options.doDiploid == True and options.doPooled == True:
            print "Usage: -b bamfile -r reffile --o output --doDiploid | --doPooled [--maxRead | --filerHaplotypes | --maxHap]" 
            sys.exit(0)
        elif options.doDiploid == True and options.doPooled == False:
            opt1 = " --doDiploid "
            opt4 = "mergeOutputDiploid.py "
        else:
            opt1 = " --doPooled " 
            opt4 = "mergeOutputPooled.py "
        opt2 = ""  
        opt3 = "" 
        
        if options.quiet == True:
            opt2 = " --quiet "
            opt3 = " 2>&1"

        cmd_s1 = "dindel --analysis getCIGARindels --bamFile " + options.bamfile + " --ref " + options.reffile + " --outputFile tmp/dindel_output" + opt2
        cmd_s2 = "python " + INSTALL_PATH  + "makeWindows.py --inputVarFile tmp/dindel_output.variants.txt --windowFilePrefix tmp/windows/t --numWindowsPerFile 1000" + opt3

        if options.bamindex <> None:
            os.system("mv " + options.bamindex + " " + options.bamfile + ".bai")


        os.system("mkdir tmp; mkdir tmp/windows; mkdir tmp/out_windows");		
        print "Stage 1 running..."
        print (cmd_s1)
        os.system(cmd_s1);
        print "Stage 2 running..."
        os.system(cmd_s2);
        print (cmd_s2)
        n = 0
        print "Stage 3 running..."
        for infile in glob.glob( os.path.join("tmp/windows/", 't*.txt')):
            cmd_s3 = "dindel --analysis indels " + opt1 + " --bamFile " + options.bamfile + " --ref " + options.reffile + " --varFile " + infile + " --libFile tmp/dindel_output.libraries.txt --outputFile tmp/out_windows/t" + str(n) + ".txt" + opt2 + opt3
            print (cmd_s3)
            os.system(cmd_s3)
            n = n + 1

        print "Stage 4 running..."
        os.system("ls tmp/out_windows/* > tmp/stage2_outputfiles.txt");
        cmd_s4 = "python " + INSTALL_PATH + opt4 + " --inputFiles tmp/stage2_outputfiles.txt --outputFile " + options.output + " --ref " + options.reffile + opt3
        print (cmd_s4)
        os.system(cmd_s4);	
        # os.system("rm tmp -rf");

        if options.bamindex <> None:
            os.system("mv " + options.bamfile + ".bai " +  options.bamindex)

if __name__ == '__main__':
    main()


