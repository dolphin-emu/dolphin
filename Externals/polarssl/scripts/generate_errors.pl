#!/usr/bin/perl
#

use strict;

my $include_dir = shift or die "Missing include directory";
my $data_dir = shift or die "Missing data directory";
my $error_file = shift or die "Missing destination file";
my $error_format_file = $data_dir.'/error.fmt';

my @low_level_modules = ( "AES", "ASN1", "BLOWFISH", "CAMELLIA", "BIGNUM",
                          "BASE64", "XTEA", "PBKDF2",
                          "PADLOCK", "DES", "NET", "CTR_DRBG", "ENTROPY",
                          "MD2", "MD4", "MD5", "SHA1", "SHA2", "SHA4", "GCM" );
my @high_level_modules = ( "PEM", "X509", "DHM", "RSA", "MD", "CIPHER", "SSL",
                           "PKCS12", "PKCS5" );

my $line_separator = $/;
undef $/;

open(FORMAT_FILE, "$error_format_file") or die "Opening error format file '$error_format_file': $!";
my $error_format = <FORMAT_FILE>;
close(FORMAT_FILE);

$/ = $line_separator;

open(GREP, "/bin/grep \"define POLARSSL_ERR_\" $include_dir/* |") || die("Failure when calling grep: $!");

my $ll_old_define = "";
my $hl_old_define = "";

my $ll_code_check = "";
my $hl_code_check = "";

my $headers = "";

while (my $line = <GREP>)
{
    my ($error_name, $error_code) = $line =~ /(POLARSSL_ERR_\w+)\s+\-(0x\w+)/;
    my ($description) = $line =~ /\/\*\*< (.*?)\.? \*\//;
    $description =~ s/\\/\\\\/g;
    $description = "DESCRIPTION MISSING" if ($description eq "");

    my ($module_name) = $error_name =~ /^POLARSSL_ERR_([^_]+)/;

    # Fix faulty ones
    $module_name = "BIGNUM" if ($module_name eq "MPI");
    $module_name = "CTR_DRBG" if ($module_name eq "CTR");

    my $define_name = $module_name;
    $define_name = "X509_PARSE" if ($define_name eq "X509");
    $define_name = "ASN1_PARSE" if ($define_name eq "ASN1");
    $define_name = "SSL_TLS" if ($define_name eq "SSL");

    my $include_name = $module_name;
    $include_name =~ tr/A-Z/a-z/;
    $include_name = "" if ($include_name eq "asn1");

    my $found_ll = grep $_ eq $module_name, @low_level_modules;
    my $found_hl = grep $_ eq $module_name, @high_level_modules;
    if (!$found_ll && !$found_hl)
    {
        printf("Error: Do not know how to handle: $module_name\n");
        exit 1;
    }

    my $code_check;
    my $old_define;
    my $white_space;

    if ($found_ll)
    {
        $code_check = \$ll_code_check;
        $old_define = \$ll_old_define;
        $white_space = '    ';
    }
    else
    {
        $code_check = \$hl_code_check;
        $old_define = \$hl_old_define;
        $white_space = '        ';
    }

    if ($define_name ne ${$old_define})
    {
        if (${$old_define} ne "")
        {
            ${$code_check} .= "#endif /* POLARSSL_${$old_define}_C */\n\n";
        }

        ${$code_check} .= "#if defined(POLARSSL_${define_name}_C)\n";
        $headers .= "#if defined(POLARSSL_${define_name}_C)\n".
                    "#include \"polarssl/${include_name}.h\"\n".
                    "#endif\n\n" if ($include_name ne "");
        ${$old_define} = $define_name;
    }

    if ($error_name eq "POLARSSL_ERR_SSL_FATAL_ALERT_MESSAGE")
    {
        ${$code_check} .= "${white_space}if( use_ret == -($error_name) )\n".
                          "${white_space}\{\n".
                          "${white_space}    snprintf( buf, buflen, \"$module_name - $description\" );\n".
                          "${white_space}    return;\n".
                          "${white_space}}\n"
    }
    else
    {
        ${$code_check} .= "${white_space}if( use_ret == -($error_name) )\n".
                          "${white_space}    snprintf( buf, buflen, \"$module_name - $description\" );\n"
    }
};

if ($ll_old_define ne "")
{
    $ll_code_check .= "#endif /* POLARSSL_${ll_old_define}_C */\n\n";
}
if ($hl_old_define ne "")
{
    $hl_code_check .= "#endif /* POLARSSL_${hl_old_define}_C */\n\n";
}

$error_format =~ s/HEADER_INCLUDED\n/$headers/g;
$error_format =~ s/LOW_LEVEL_CODE_CHECKS\n/$ll_code_check/g;
$error_format =~ s/HIGH_LEVEL_CODE_CHECKS\n/$hl_code_check/g;

open(ERROR_FILE, ">$error_file") or die "Opening destination file '$error_file': $!";
print ERROR_FILE $error_format;
close(ERROR_FILE);
