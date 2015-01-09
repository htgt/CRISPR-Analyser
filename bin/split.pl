#!/usr/bin/perl

use strict;
use warnings;

use Getopt::Long;
use Pod::Usage;

my ( $max_lines, $file_index, $prefix ) = ( 7000, 1, "crisprs_" );
GetOptions(
    'help'          => sub { pod2usage( 1 ) },
    'lines=i'       => \$max_lines,
    'prefix=s'      => \$prefix,
    'start-index=i' => \$file_index,
) or pod2usage( 2 );

pod2usage( 2 ) unless @ARGV;

my $fh;
my $current_line = 0;
while ( my $line = <> ) {
    if ( ! $fh ) {
        $current_line = 0;
        my $filename = $prefix . $file_index++;
        open $fh, ">", $filename or die "Couldn't open $filename: $!";
    }

    print $fh $line;

    #if we reached the max number of lines then close this filehandle
    if ( ++$current_line >= $max_lines ) {
        close $fh or die "Couldn't close filehandle: $!";
        undef $fh; #make it undefined so we re-open on next loop
    }
}

1;

__END__

=head1 NAME

split.pl - split a file into multiple files (like unix split but specifically for crispr ids)

=head1 SYNOPSIS

split.pl [options] <file.txt>

    --lines              the number of lines to have in each file [default is 7000]
    --prefix             the output filename prefix [default is crisprs_]
    --start-index        the file number to start at [default is 1]
    --help               show this dialog

Example usage:

split.pl --lines 4000 test.txt

This will split the file test.txt into files with 4k lines each, named crisprs_1, crisprs_2 etc.

=head1 DESCRIPTION

If you have a list of CRISPR IDs that you want to farm out over multiple jobs this is
the script for you

The UNIX split command does this almost perfectly, but it has a leading 0 before the single digit
files, so you have to do some fiddling. You also have to remember the split syntax,
for this you just run 'perl split.pl ids.txt' and you're ready to go

=head AUTHOR

Alex Hodgkins

=cut
