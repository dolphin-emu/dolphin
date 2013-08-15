killall -q openssl ssl_server ssl_server2

MODES="ssl3 tls1 tls1_1 tls1_2"
VERIFIES="NO YES"
OPENSSL=openssl

for VERIFY in $VERIFIES;
do
if [ "X$VERIFY" = "XYES" ];
then
    P_SERVER_ARGS="auth_mode=required crt_file=data_files/server1.crt key_file=data_files/server1.key ca_file=data_files/test-ca.crt"
    P_CLIENT_ARGS="crt_file=data_files/server2.crt key_file=data_files/server2.key ca_file=data_files/test-ca.crt"
    O_SERVER_ARGS="-verify 10 -CAfile data_files/test-ca.crt -cert data_files/server1.crt -key data_files/server1.key"
    O_CLIENT_ARGS="-cert data_files/server2.crt -key data_files/server2.key -CAfile data_files/test-ca.crt"
fi

for MODE in $MODES;
do
echo "Running for $MODE (Verify: $VERIFY)"
echo "-----------"

P_CIPHERS="                                 \
    TLS-DHE-RSA-WITH-AES-128-CBC-SHA        \
    TLS-DHE-RSA-WITH-AES-256-CBC-SHA        \
    TLS-DHE-RSA-WITH-CAMELLIA-128-CBC-SHA   \
    TLS-DHE-RSA-WITH-CAMELLIA-256-CBC-SHA   \
    TLS-DHE-RSA-WITH-3DES-EDE-CBC-SHA       \
    TLS-RSA-WITH-AES-256-CBC-SHA            \
    TLS-RSA-WITH-CAMELLIA-256-CBC-SHA       \
    TLS-RSA-WITH-AES-128-CBC-SHA            \
    TLS-RSA-WITH-CAMELLIA-128-CBC-SHA       \
    TLS-RSA-WITH-3DES-EDE-CBC-SHA           \
    TLS-RSA-WITH-RC4-128-SHA                \
    TLS-RSA-WITH-RC4-128-MD5                \
    TLS-RSA-WITH-NULL-MD5                   \
    TLS-RSA-WITH-NULL-SHA                   \
    TLS-RSA-WITH-DES-CBC-SHA                \
    TLS-DHE-RSA-WITH-DES-CBC-SHA            \
    "

O_CIPHERS="                         \
    DHE-RSA-AES128-SHA              \
    DHE-RSA-AES256-SHA              \
    DHE-RSA-CAMELLIA128-SHA         \
    DHE-RSA-CAMELLIA256-SHA         \
    EDH-RSA-DES-CBC3-SHA            \
    AES256-SHA                      \
    CAMELLIA256-SHA                 \
    AES128-SHA                      \
    CAMELLIA128-SHA                 \
    DES-CBC3-SHA                    \
    RC4-SHA                         \
    RC4-MD5                         \
    NULL-MD5                        \
    NULL-SHA                        \
    DES-CBC-SHA                     \
    EDH-RSA-DES-CBC-SHA             \
    "

# Also add SHA256 ciphersuites
#
if [ "$MODE" = "tls1_2" ];
then
    P_CIPHERS="$P_CIPHERS                       \
        TLS-RSA-WITH-NULL-SHA256                \
        TLS-RSA-WITH-AES-128-CBC-SHA256         \
        TLS-DHE-RSA-WITH-AES-128-CBC-SHA256     \
        TLS-RSA-WITH-AES-256-CBC-SHA256         \
        TLS-DHE-RSA-WITH-AES-256-CBC-SHA256     \
        "

    O_CIPHERS="$O_CIPHERS           \
        NULL-SHA256                 \
        AES128-SHA256               \
        DHE-RSA-AES128-SHA256       \
        AES256-SHA256               \
        DHE-RSA-AES256-SHA256       \
        "

    P_CIPHERS="$P_CIPHERS                   \
        TLS-RSA-WITH-AES-128-GCM-SHA256     \
        TLS-RSA-WITH-AES-256-GCM-SHA384     \
        TLS-DHE-RSA-WITH-AES-128-GCM-SHA256 \
        TLS-DHE-RSA-WITH-AES-256-GCM-SHA384 \
        "

    O_CIPHERS="$O_CIPHERS           \
        AES128-GCM-SHA256           \
        DHE-RSA-AES128-GCM-SHA256   \
        AES256-GCM-SHA384           \
        DHE-RSA-AES256-GCM-SHA384   \
        "
fi

$OPENSSL s_server -cert data_files/server2.crt -key data_files/server2.key -www -quiet -cipher NULL,ALL $O_SERVER_ARGS -$MODE &
PROCESS_ID=$!

sleep 1

for i in $P_CIPHERS;
do
    RESULT="$( ../programs/ssl/ssl_client2 $P_CLIENT_ARGS force_ciphersuite=$i )"
    EXIT=$?
    echo -n "OpenSSL Server - PolarSSL Client - $i : $EXIT - "
    if [ "$EXIT" = "2" ];
    then
        echo Ciphersuite not supported in client
    elif [ "$EXIT" != "0" ];
    then
        echo Failed
        echo $RESULT
    else
        echo Success
    fi
done
kill $PROCESS_ID

../programs/ssl/ssl_server2 $P_SERVER_ARGS > /dev/null &
PROCESS_ID=$!

sleep 1

for i in $O_CIPHERS;
do
    RESULT="$( ( echo -e 'GET HTTP/1.0'; echo; sleep 1 ) | $OPENSSL s_client -$MODE -cipher $i $O_CLIENT_ARGS 2>&1 )"
    EXIT=$?
    echo -n "PolarSSL Server - OpenSSL Client - $i : $EXIT - "

    if [ "$EXIT" != "0" ];
    then
        SUPPORTED="$( echo $RESULT | grep 'Cipher is (NONE)' )"
        if [ "X$SUPPORTED" != "X" ]
        then
            echo "Ciphersuite not supported in server"
        else
            echo Failed
            echo ../programs/ssl/ssl_server2 $P_SERVER_ARGS 
            echo $OPENSSL s_client -$MODE -cipher $i $O_CLIENT_ARGS 
            echo $RESULT
        fi
    else
        echo Success
    fi
done

kill $PROCESS_ID

../programs/ssl/ssl_server2 $P_SERVER_ARGS > /dev/null &
PROCESS_ID=$!

sleep 1

# OpenSSL does not support RFC5246 Camellia ciphers with SHA256
# Add for PolarSSL only test, which does support them.
#
if [ "$MODE" = "tls1_2" ];
then
    P_CIPHERS="$P_CIPHERS                        \
        TLS-RSA-WITH-CAMELLIA-128-CBC-SHA256     \
        TLS-DHE-RSA-WITH-CAMELLIA-128-CBC-SHA256 \
        TLS-RSA-WITH-CAMELLIA-256-CBC-SHA256     \
        TLS-DHE-RSA-WITH-CAMELLIA-256-CBC-SHA256 \
        "
fi

for i in $P_CIPHERS;
do
    RESULT="$( ../programs/ssl/ssl_client2 force_ciphersuite=$i $P_CLIENT_ARGS )"
    EXIT=$?
    echo -n "PolarSSL Server - PolarSSL Client - $i : $EXIT - "
    if [ "$EXIT" = "2" ];
    then
        echo Ciphersuite not supported in client
    elif [ "$EXIT" != "0" ];
    then
        echo Failed
        echo $RESULT
    else
        echo Success
    fi
done
kill $PROCESS_ID

done
done
