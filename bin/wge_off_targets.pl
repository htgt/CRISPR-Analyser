#!/usr/bin/perl

use strict;
use warnings;

use Log::Log4perl qw( :easy );
use UUID::Tiny ':std';
use autodie;
use feature qw( say );

use IPC::System::Simple qw( run );
use Try::Tiny;
use File::Path qw( remove_tree );
use Path::Class;

#use WGE::Model::DB;
use WGE::Util::PersistCrisprs::TSV;

BEGIN { Log::Log4perl->easy_init( $DEBUG ) }

die "You must set WGE_REST_CLIENT_CONFIG" unless $ENV{WGE_REST_CLIENT_CONFIG};

die "Usage: run_batch_crisprs.pl <species> <ids.txt>" unless @ARGV >= 2;

my $species = ucfirst( lc shift );

my %INDEX_FILES = (
    Mouse  => $ENV{'GRCM38_CRISPR_INDEX'} || '/lustre/scratch110/sanger/team87/crispr_indexes/GRCm38_index.bin',
    Human  => $ENV{'GRCH37_CRISPR_INDEX'} || '/lustre/scratch110/sanger/team87/crispr_indexes/GRCh37_index.bin',
    Grch38 => $ENV{'GRCH38_CRISPR_INDEX'} || '/lustre/scratch110/sanger/team87/crispr_indexes/GRCh38_index.bin',
    Pig    => $ENV{'SSCROFA_CRISPR_INDEX'} || '/lustre/scratch109/sanger/ah19/crispr_indexes/pig.bin',
);

die "Invalid species '$species'" unless exists $INDEX_FILES{$species};

my $index_file = file( $INDEX_FILES{$species} );
die "$index_file doesn't exist" unless -f $index_file;

my $batch_size = 1000;
my $run_dir = dir( $ENV{'OFF_TARGET_RUN_DIR'} || "/lustre/scratch110/sanger/ah19/wge_exons/" );
my $log_dir = dir( $ENV{'OFF_TARGET_LOG_DIR'} || "/lustre/scratch110/sanger/ah19/crispr_logs/" );


my $num_passed = 0;
my $num_batches = 0;

{
    my @batch; #local

    DEBUG "Processing file " . $ARGV[0] . " for $species";

    while ( my $line = <> ) {
        chomp $line;

        next unless $line; #skip blank line

        die "ids must be numeric" unless $line =~ /\d+/;

        push @batch, $line;

        #wait until we have enough
        next if @batch < $batch_size;

        WARN "File is $ARGV";

        run_batch( @batch );

        #reset for next batch
        @batch = ();
    }

    if ( @batch ) {
        DEBUG "Running remaining " . scalar( @batch ) . " items";
        run_batch( @batch );
    }

    WARN "All crisprs complete.";
    WARN "$num_passed jobs passed";
}

sub run_batch {
    my @batch = @_;

    DEBUG "Running batch " . ++$num_batches;

    #use uuids because i'm lazy
    my $id = create_uuid_as_string();

    my $dir = $run_dir->subdir( $id );
    $dir->mkpath();

    DEBUG "Working directory is $dir";

    #dump @args so we can get useful data from the run dir
    {
        my $fh = $dir->file( "info.txt" )->openw();
        print $fh "File: $ARGV\nSpecies: $species\nBatch: $num_batches\nBatch size: $batch_size\n";
        close $fh;

        #dump the ids into a text file
        $fh = $dir->file( "ids.txt" )->openw();
        print $fh join "\n", @batch;
    }

    DEBUG "Run id is $id";
    DEBUG "Running batch with " . scalar( @batch ) . " ids";

    my $outfile = $dir->file('output.tsv');
    my $failed = 0;

    try {
        my $cmd = join " ", (
            $ENV{'OFF_TARGET_BINARY_PATH'} || "/nfs/team87/CRISPR-Analyser/bin/crispr_analyser",
            "align",
            "-i", $index_file->stringify,
            @batch,
            '>', $outfile->stringify,
        );

        DEBUG "Running $cmd";

        my $retval = run $cmd;

        DEBUG "returned $retval";

        $num_passed++;

        DEBUG "Persisting $outfile";

        WGE::Util::PersistCrisprs::TSV->new_with_config(
            configfile => $ENV{WGE_REST_CLIENT_CONFIG},
            species    => $species,
            dry_run    => 0, #never dry run -- should add cmd line options
            tsv_file   => $outfile,
        )->execute;

        DEBUG "Everything successful, deleting $dir";
        #always delete the folder
        my $num_deleted = $dir->rmtree();

        WARN "Deleted $num_deleted files.";
    }
    catch {
        WARN $_;
        WARN "$dir was not deleted";
        $failed = 1;
    };
}

1;
