#!/bin/bash
# bash> doip_client.sh -d ./swdl_tar -c 1 -h 192.168.2.10

##################################

# author: yangweicai
# date  : 2023/06/14 15:00:00

##################################

CFG_TAR_PATH="./swdl_tar"
CFG_IP="192.168.2.10"
CFG_TOTAL_CNT=1

VERSION="doip_client 20230407 10:00:00"

trap "" PIPE
trap "" SIGWINCH

sudo sysctl -w net.ipv4.tcp_syn_retries=1 >/dev/null

CUR_FILE="$0"
TOP_SHELL_PID="$$"

if [[ "$0" =~ "bash" ]];then # source
    DIR=.
    CUR_FILE=""
else
    DIR=$(
        cd "$(dirname "$0")" || exit 1
        pwd
    )
fi

# DOIP SOCKET PORT

TCP_DATA=13400
# UDP_DISCOVERY=13400
# UDP_DISCOVERY_SEND=13400

LOG_PATH="$DIR/doip_client_logs"
FILE_LOG="$LOG_PATH/doip_client.log"

TMP_PATH="$LOG_PATH/tmp" # use tmpfs
mkdir -p "$TMP_PATH"

if [ x"$(df -h | grep -o "$TMP_PATH")" != x"$TMP_PATH" ];then
    sudo mount -t tmpfs none "$TMP_PATH"
fi

FILE_SOCK_ERR="$TMP_PATH/file_sock_err"
FILE_SOCK_SEND="$TMP_PATH/file_sock_send"
FILE_SOCK_RECV_PID="$TMP_PATH/file_sock_recv_pid"
FILE_SOCK_RECV_RES="$TMP_PATH/file_sock_recv_res"
FILE_SOCK_RECV_SIZE_PID="$TMP_PATH/file_sock_recv_size_pid"
FILE_SOCK_RECV_SIZE="$TMP_PATH/file_sock_recv_size"
FILE_RECV_DIAG_MSG_RES="$TMP_PATH/file_recv_diag_msg_res"
FILE_OTA_PROG_STATUS="$TMP_PATH/file_ota_prog_status"
FILE_TOP_PID="$TMP_PATH/file_top_pid"
IP=""
TAR_PATH=""
TOTAL_CNT=""
UPGRADE_TAR_FILE=""
TAR_FILES=()

G_VAR_MSG_DOIP_PARSE_LEN=0

########################## utility function #############################################

LOG_LEVEL="E:W:I:S" # E:W:I:S:D
MAX_SIZE_PRINT=128

function COLOR_R() {
    local p=$*
    local last=${p:0-2}
    [[ x"$last" == x"\n" ]] && p=${p:0:-2} || last=""
    printf "%s" "\033[31m$p\033[0m${last}"
}

function COLOR_G() {
    local p=$*
    local last=${p:0-2}
    [[ x"$last" == x"\n" ]] && p=${p:0:-2} || last=""
    printf "%s" "\033[32m$p\033[0m${last}"
}

function COLOR_Y() {
    local p=$*
    local last=${p:0-2}
    [[ x"$last" == x"\n" ]] && p=${p:0:-2} || last=""
    printf "%s" "\033[33m$p\033[0m${last}"
}

function COLOR_NONE() {
    printf "%s" "$*"
}

declare -A LEVEL_COLOR=(
    ["E"]=COLOR_R
    ["W"]=COLOR_Y
    ["I"]=COLOR_NONE
    ["S"]=COLOR_G
    ["D"]=COLOR_NONE
)

__print_cls() {
    local level="$1"
    local p="$2"
    local tm
    local last
    local call_line="[L${BASH_LINENO[1]}]"
    local blank=" "
    local info;

    tm="[$(date '+%02d%H:%M:%S')]"
    # output file
    info=$(${LEVEL_COLOR[$level]} "[${level}]${blank}${p}")

    last=${p:0-2}

    if [ x"$last" != x"\n" ];then
        last="\n"
    fi

    printf "%b" "${tm}[$TOP_SHELL_PID]${call_line}${info}${last}" >> "$FILE_LOG"

    # output console
    if [[ ! $LOG_LEVEL =~ $level ]];then
        return 0
    fi

    last=${p:0-2}
    max_len=$(stty size 2>/dev/null |awk '{print $2}')
    [ -z "$max_len" ] && max_len=0
    [[ $max_len -gt 0 ]] && max_len=$((max_len - ${#tm} -${#call_line} - ${#level} - ${#blank} - 2))
    local ignore=""
    [[ $max_len -gt 0 ]] && [[ $max_len -lt ${#p} ]] && ignore=" ..." && max_len=$((max_len - ${#ignore}))
    [[ $max_len -gt 0 ]] && p=${p:0:$max_len}

    if [[ x"$last" != x"\n" ]] || [[ -z "$ignore" ]];then 
        last=""
    fi

    info=$(${LEVEL_COLOR[$level]} "[${level}]${blank}${p}${ignore}")
    printf "%b" "\r\033[2K${tm}${call_line}${info}${last}"
}

function print_error() {
    local i=$1
    [[ -z "$i" ]] && echo && return 0
    local info=${i:0:$MAX_SIZE_PRINT}
    [[ ${#i} -gt $MAX_SIZE_PRINT ]] && i="$info ..."
    __print_cls "E" "$i"
}

function print_warning() {
    local i=$1
    [[ -z "$i" ]] && echo && return 0
    local info=${i:0:$MAX_SIZE_PRINT}
    [[ ${#i} -gt $MAX_SIZE_PRINT ]] && i="$info ..."
    __print_cls "W" "$i"
}

function print_debug() {
    local i=$1
    [[ -z "$i" ]] && echo && return 0
    local info=${i:0:$MAX_SIZE_PRINT}
    [[ ${#i} -gt $MAX_SIZE_PRINT ]] && i="$info ..."
    __print_cls "D" "$i"
}

function print_info() {
    local i=$1
    [[ -z "$i" ]] && echo && return 0
    local info=${i:0:$MAX_SIZE_PRINT}
    [[ ${#i} -gt $MAX_SIZE_PRINT ]] && i="$info ..."
    __print_cls "I" "$i"
}

function print_success() {
    local i=$1
    [[ -z "$i" ]] && echo && return 0
    local info=${i:0:$MAX_SIZE_PRINT}
    [[ ${#i} -gt $MAX_SIZE_PRINT ]] && i="$info ..."
    __print_cls "S" "$i"
}

# $1: bytes
# $2: offset begin from low bytes (0 ... N)
# $3: bytes counts to get

function get_byte() {
    local params=$#
    local bytes="$1"
    local offset=$(($2))
    local len="$3"

    if [ "$params" -lt 3 ]; then
        bytes="$(cat)"
        offset=$1
        len=$2
    fi

    [[ $len -lt 0 ]] && len=""

    offset=$((offset + 1))

    [[ $params -ne 0 ]] && [[ ${#bytes}%2 -ne 0 ]] && byte="0$bytes"

    if [ -n "$len" ]; then
        echo -ne "${bytes:0-$((offset * 2)):$((len * 2))}"
    else
        echo -ne "${bytes:0-$((offset * 2))}"
    fi
}

function str_to_hex() {

    if [ $# -ne 0 ];then
        echo -ne "$@" | xxd -r -ps
    else
        xxd -r -ps
    fi
}

function hex_to_str() {

    if [ $# -ne 0 ];then
        echo -ne "$@" | xxd -ps | tr -d '\n '
    else
        xxd -ps | tr -d '\n '
    fi
}

# 0xabcd, 123, 0x0123

function hex_calc() {
    local res="0"
    [[ $# -ne 0 ]] && res=$(printf "%02x" $(($@))) || res=$(printf "%02x" $(($(cat))))
    rt=$?
    [[ ${#res}%2 -ne 0 ]] && res="0$res"
    echo -ne "$res"
    return $rt
}

function ping_test() {
	local call_line="[L$1]"
	local ping_max=$2
	local i=1
	local rt=255

	while true
	do
		# print_info "[Line:$LINENO] ping $ip ..."
        local pin
		local valid
        local loss

        pin=$(ping -w 2 -c 1 $IP 2>&1)
		valid=$(echo -e "$pin" | sed -n -r -e 's/.*(Destination Host Unreachable).*/\1/p')
		loss=$(echo -e "$pin" | sed -n -r -e 's/.* ([0-9]{1,3})% packet loss.*/\1/p')

		if [ "$loss" -eq 100 ];then
            rt=255

			if [ "$i" -lt "$ping_max" ];then
				print_warning "$call_line warning($i) ping($IP) $valid loss:${loss}%, retry ...\n" >&2
				i=$((i+1))
			else
				#print_info "" >&2
				print_warning "$call_line error($i) ping($IP) max count ...\n" >&2
				#kill_pid_children $TOP_SCRIPT_PID
                break
			fi
			sleep 0.1
		else
            rt=0
			break
		fi
	done

	return $rt
}

function sock_tcp_connect() {
    local i=1
    local max_i=$1
    local rt=255

    while [[ $rt -ne 0 ]] && [[ $i -le $max_i ]];do
        exist_sock_recv_proc 1
        exist_sock_recv_size_proc 1
        exec 2>"$FILE_SOCK_ERR" 8<>/dev/tcp/$IP/$TCP_DATA
        rt=$?
        exec 2>&1
        if [ $rt -ne 0 ];then
            print_warning "sock_tcp_connect($i/$max_i) $(head -n1 < "$FILE_SOCK_ERR")" && sleep 1
            i=$((i+1))
        fi
    done

    if [ $rt -ne 0 ];then
        print_error "sock_tcp_connect($max_i) $(head -n1 < "$FILE_SOCK_ERR")\n"
    else
        print_success "sock_tcp_connect($i/$max_i) ok !\n"
    fi

    return $rt
}

function sock_tcp_send() {
    local file=$1
    local file_sock_recv=$2
    local rt=255
    local tmout=6

    echo -ne "" > "$file_sock_recv"

    [[ -s $FILE_SOCK_ERR ]] && echo "" > "$FILE_SOCK_ERR"

    if [ -n "$file" ];then
        timeout $tmout cat "$file" 2>"$FILE_SOCK_ERR" >&8
        rt=${PIPESTATUS[0]}
        if [ "$rt" -eq 124 ];then
            echo "sock_tcp_send: Connection timed out" > "$FILE_SOCK_ERR"
        fi
    else
        cat 2>"$FILE_SOCK_ERR" >&8
        rt=${PIPESTATUS[0]}
    fi
    [[ $rt -ne 0 ]] && print_error "tcp send error rt:$rt, $(head -n1 < "$FILE_SOCK_ERR")\n"
    return "$rt"
}

function get_sock_recv() {
    cat >> "$FILE_SOCK_RECV_RES" 2> "$FILE_SOCK_ERR" <&8
    print_error "get_sock_recv exit, pid:$(cat "$FILE_SOCK_RECV_PID") !\n"
    sleep 1
    echo -ne "" > "$FILE_SOCK_RECV_PID"
    echo -ne "" > "$file"
}

function get_sock_recv_size() {

    local file=$FILE_SOCK_RECV_RES

    while true; do
        local bytes=$(($(wc -c "$file" | awk '{print $1}')))
        echo "$bytes" > "$FILE_SOCK_RECV_SIZE"
        sync -f "$file"
        sync -f "$FILE_SOCK_RECV_SIZE"
        sleep 0.01
    done
}

function sock_tcp_recv() {
    local file=$1
    local i=0
    local max_i=50
    local bytes_recv=0
    local tmout=0.1
    local rt=0
    local pid=''
    local bytes_old=0

    [[ -z "$file" ]] && return 255
    [[ -s "$FILE_SOCK_ERR" ]] && echo "" > "$FILE_SOCK_ERR"

    exist_sock_recv_proc 0
    exist_sock_recv_size_proc 0

    bytes_old=$(( $(cat "$FILE_SOCK_RECV_SIZE") ))

    rt=0

    while [[ $rt -eq 0 ]] && [[ $bytes_recv -le $bytes_old ]] && [[ $i -lt $max_i ]];do

        if [[ -s $FILE_SOCK_ERR ]]; then
            print_error "sock_tcp_recv($i/$max_i) error, $(head -n1 < "$FILE_SOCK_ERR")\n"
            rt=1
            break
        fi

        bytes_recv=$(( $(cat "$FILE_SOCK_RECV_SIZE") ))

        i=$((i+1))

        if [[ $bytes_recv -eq 0 ]];then
            print_warning "sock_tcp_recv($i/$max_i) recv $bytes_recv bytes, old: $bytes_old, continue ..."
            ping_test $LINENO 1
            rt=$?
            if [[ $rt -eq 0 ]];then
                sleep $tmout
            else
                ping_test $LINENO 10
                rt=$?
                if [ $rt -ne 0 ];then
                    local err="ping: Connection timed out !"
                    echo "$err" > "$FILE_SOCK_ERR"
                    print_error "$err"
                fi
            fi
        fi
    done

    return $rt
}

function sock_tcp_close() {
    print_info "sock_tcp_close !\n"
	exist_sock_recv_proc 1
	exist_sock_recv_size_proc 1
    exec 8>&-
	sleep 5
}

########################## DOIP protocol define ################################

DOIP_HEADER_SIZE=8 # header: [ver:1][rev:1][type:2][len:4]
DOIP_PROTOCOL_VERSION=2

# c385
DOIP_PROTOCOL_SA_2_BYTES=0f00
DOIP_PROTOCOL_TA_2_BYTES=e4df
DOIP_PROTOCOL_TA_ECU_2_BYTES=0017

# Payload type

#DOIP_M_GENERIC_HEADER_NACK=0x0000
#DOIP_M_VEHICLE_IDENTIFICATION_REQUEST=0x0001
#DOIP_M_VEHICLE_IDENTIFICATION_REQUEST_EID=0x0002
#DOIP_M_VEHICLE_IDENTIFICATION_REQUEST_VIN=0x0003
#DOIP_M_VEHICLE_IDENTIFICAITON_RESPONSE=0x0004
#DOIP_M_VEHICLE_ANNOUNCEMENT=0x0004
#DOIP_M_ENTITY_STATUS_REQUEST=0x4001
#OIP_M_ENTITY_STATUS_RESPONSE=0x4002
#DOIP_M_DIAG_POWER_MODE_REQEUST=0x4003
#DOIP_M_DIAG_POWER_MODE_RESPONSE=0x4004

DOIP_PROTO_ALIVE_CHECK_REQ=0x0007   # alive check request (DOIP node -> tester)
DOIP_PROTO_ALIVE_CHECK_RESP=0x0008  # alive check response (tester -> DOIP node)

DOIP_PROTO_ROUTING_ACTIVATION_REQ=0x0005
DOIP_PROTO_ROUTING_ACTIVATION_RESP=0x0006
DOIP_PROTO_DIAGNOSTIC_REQ=0x8001
DOIP_PROTO_DIAGNOSTIC_REQ_ACK=0x8002
DOIP_PROTO_DIAGNOSTIC_REQ_NACK=0x8003

DOIP_RESP_CODE_NEGATIVE=0x7F
DOIP_RESP_CODE_PENDING=0x78
RSID_OFFSET=0x40

# Generic doip header negative acknowledge codes

# DOIP_E_INCORRECT_PATTERN_FORMAT=0x00
# DOIP_E_UNKNOWN_PAYLOAD_TYPE=0x01
# DOIP_E_MESSAGE_TO_LARGE=0x02
# DOIP_E_OUT_OF_MEMORY=0x03
# DOIP_E_INVALID_PAYLOAD_LENGTH=0x04

# Routing activation type

# DOIP_ACTIVATION_DEFAULT=0x00
# DOIP_ACTIVATION_WWH_OBD=0x01
# DOIP_ACTIVATION_CENTRAL_SECURITY=0xE0

# Routing activation response code

# DOIP_E_UNKNOWN_SA=0x00
# DOIP_E_ALL_SOCKETS_REGISTERED=0x01
# DOIP_E_SA_DIFFERENT=0x02
# DOIP_E_SA_ALREADY_ACTIVATED=0x03
# DOIP_E_MISSING_AUTHENTICATION=0x04
# DOIP_E_REJECT_CONFIRMATION=0x05
# DOIP_E_UNSUPPORTED_ROUTING_ACTIVATION_TYPE=0x06
# DOIP_E_ACTIVATE_SUCCESS=0x10
# DOIP_E_CONFRIMATION_REQUIRED=0x11

# Diagnostic message negative acknowledge codes

# DOIP_E_DIAG_INVALID_SA=0x02         # Invalid Source Address
# DOIP_E_DIAG_UNKNOWN_TA=0x03         # Unknown Target Address
# DOIP_E_DIAG_MESSAGE_TO_LARGE=0x04   # Diagnostic Message too large
# DOIP_E_DIAG_OUT_OF_MEMORY=0x05      # Out of memory
# DOIP_E_DIAG_TARGET_UNREACHABLE=0x06 # Target unreachable
# DOIP_E_DIAG_UNKNOWN_NETWORK=0x07    # Unknown network
# DOIP_E_DIAG_TP_ERROR=0x08           # Transport protocol error

# Vehicle identification type

# DOIP_IDENTIFICATION_ALL=0x00
# DOIP_IDENTIFICATION_VIN=0x01
# DOIP_IDENTIFICATION_EID=0x02

########################## DOIP request function ################################

function msg_doip_head() {
    local cmd=$1 #8001
    # local is_func_addr=$2
    local payload_len=$3

    #local TAR_ADDR=0x0017
    #[[ $is_func_addr -eq 1 ]] && TAR_ADDR=0xe4df

    local bytes_buffer=(
        $(hex_calc $DOIP_PROTOCOL_VERSION)
        $(hex_calc ~$DOIP_PROTOCOL_VERSION | get_byte)
        $(hex_calc "$cmd >> 8" | get_byte)
        $(hex_calc "$cmd" | get_byte)
        $(hex_calc "$payload_len >> 24" | get_byte)
        $(hex_calc "$payload_len >> 16" | get_byte)
        $(hex_calc "$payload_len >> 8" | get_byte)
        $(hex_calc "$payload_len" | get_byte)
    )

    echo -ne "${bytes_buffer[*]}" | tr -d " " #| xxd -r -ps | xxd -ps -c1 | xargs
}

function alive_check_resp() {
    local iMax=1
    local i=0
    local rt=0

    while [[ $rt -eq 0 ]] && [[ $i -lt $iMax ]];do
        i=$((i+1))
        print_info "call ${FUNCNAME[0]}"
        msg_doip_send "$(msg_doip_head $DOIP_PROTO_ALIVE_CHECK_RESP 0 2)${DOIP_PROTOCOL_SA_2_BYTES}" 0
        rt=$?
        if [ $rt -eq 0 ];then
            sleep 1
        fi
    done

    #routing_acti_req
    #$rt

    return $rt
}

function routing_acti_req() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(msg_doip_head $DOIP_PROTO_ROUTING_ACTIVATION_REQ 0 7)${DOIP_PROTOCOL_SA_2_BYTES}0000000000"
    local rt=$?
    return $rt
}

function routing_acti_resp() {
    # [sa:2][ta:2][resp code:1][Reserved:4]
    # [0f00][0017][10][00000000]

    local payload=$1

    if [ ${#payload} -ge 9 ]; then
        local sa
        local ta
        local resp_code

        sa=$(get_byte "$payload" -1 2)
        ta=$(get_byte "$payload" -3 2)
        resp_code=$(get_byte "$payload" -5 1)

        echo "routing_acti_resp $resp_code" >>"$FILE_RECV_DIAG_MSG_RES"

        # print_info "routing_acti_resp: sa: $sa, ta: $ta, resp: $resp_code"

        if [ x"$resp_code" == x"10" ]; then
            print_success "routing activate success\n"
            return 0
        else
            print_error "routing activate failed, code: $resp_code\n"
        fi
    else
        echo "routing_acti_resp error" >>"$FILE_RECV_DIAG_MSG_RES"
    fi

    return 255
}

function diag_msg_head() {
    local uds="$*"
    local msg
    msg="$(msg_doip_head ${DOIP_PROTO_DIAGNOSTIC_REQ} 0 $((4 + ${#uds} / 2)))${DOIP_PROTOCOL_SA_2_BYTES}${DOIP_PROTOCOL_TA_2_BYTES}${uds}"
    echo -ne "$msg"
}

function diag_msg_resp_8001() {
    # diag ack
    # [sa:2][ta:2] [uds:[sid + 0x40:1][sub sid:1]] -> [0017][0f00] [[50][03][03003201f4]] or [0017][0f00] [[0x7f][50][NRC]]

    local rt=255
    local payload=$1

    if [ ${#payload} -ge 7 ]; then
    
        local sa
        local ta
        local uds
        local sid
        sa=$(get_byte "$payload" -1 2)
        ta=$(get_byte "$payload" -3 2)
        uds=$(get_byte "$payload" -5 -1)
        sid=$(get_byte "$uds" -1 1)

        if [[ 0x$sid -eq $DOIP_RESP_CODE_NEGATIVE ]]; then
            sid=$(get_byte "$uds" -2 1)
            local nrc
            nrc=$(get_byte "$uds" -3 1)
            print_warning "${FUNCNAME[0]} sa:$sa, ta:$ta, sid:$sid, uds:$uds, nrc:$nrc\n"

            if [[ 0x$nrc -eq $DOIP_RESP_CODE_PENDING ]]; then
                # set flag to recv again
                echo "${FUNCNAME[0]} $DOIP_RESP_CODE_PENDING" >>"$FILE_RECV_DIAG_MSG_RES"
                rt=0
            else
                echo "${FUNCNAME[0]} $uds" >> "$FILE_RECV_DIAG_MSG_RES"
            fi
        elif [[ 0x$sid -ge $RSID_OFFSET ]]; then
            sid=$(hex_calc "0x$sid - $RSID_OFFSET")
            echo "${FUNCNAME[0]} $uds" >>"$FILE_RECV_DIAG_MSG_RES"
            print_info "${FUNCNAME[0]} sa:$sa, ta:$ta, sid:$sid, uds:$uds"

            if [ -n "${UDS_MSG_RESP[$sid]}" ]; then
                ${UDS_MSG_RESP[$sid]} "$(get_byte "$uds" -2 -1)"
                rt=0
            else
                print_warning "not impliment uds function, sid: $sid\n"
            fi
        fi
    else
        print_error "${FUNCNAME[0]} payload len: ${#payload}, payload:$payload\n"
        echo "${FUNCNAME[0]} error" >>"$FILE_RECV_DIAG_MSG_RES"
    fi

    return $rt
}

function diag_msg_resp_8002() {
    # diag positive ack
    # [sa:2][ta:2][code:1] -> [e4df][0f00][00]

    local rt=255
    local payload=$1

    if [ ${#payload} -ge 5 ]; then
        local sa
        local ta
        local resp_code
        sa=$(get_byte "$payload" -1 2)
        ta=$(get_byte "$payload" -3 2)
        resp_code=$(get_byte "$payload" -5 1)
        print_info "${FUNCNAME[0]}: sa: $sa, ta: $ta, resp: $resp_code"
        echo "${FUNCNAME[0]} $resp_code" >>"$FILE_RECV_DIAG_MSG_RES"
        rt=0
    else
        print_error "${FUNCNAME[0]} payload len: ${#payload}, payload:$payload\n"
        echo "${FUNCNAME[0]} error" >>"$FILE_RECV_DIAG_MSG_RES"
    fi

    return $rt
}

function diag_msg_resp_8003() {
    # diag negative ack
    # [sa:2][ta:2][code:1] -> [e4df][0f00][02]

    local rt=255
    local payload=$1

    if [ ${#payload} -ge 5 ]; then
        local sa
        local ta
        local resp_code
        sa=$(get_byte "$payload" -1 2)
        ta=$(get_byte "$payload" -3 2)
        resp_code=$(get_byte "$payload" -5 1)
        print_warning "${FUNCNAME[0]}: sa: $sa, ta: $ta, resp: $resp_code\n"
        echo "${FUNCNAME[0]} $resp_code" >>"$FILE_RECV_DIAG_MSG_RES"
        rt=255
    else
        print_error "${FUNCNAME[0]} payload len: ${#payload}, payload:$payload\n"
        echo "${FUNCNAME[0]} error" >>"$FILE_RECV_DIAG_MSG_RES"
    fi

    return $rt
}

## return ##
# parse length   : G_VAR_MSG_DOIP_PARSE_LEN (> 0); 
# error          : 255;
# msg not enough : 0

function msg_doip_parse() {

    # [02][FD][cmd:2][len:4][sa:2][ta:2][uds]

    local hex_str="$1"
    local rt=255
    local calc_ver_rev=$((0x$(hex_calc ~$DOIP_PROTOCOL_VERSION | get_byte)))

    G_VAR_MSG_DOIP_PARSE_LEN=0

    print_debug "parse bytes:$((${#hex_str}/2)), msg:$hex_str"

    while [[ ${#hex_str} -gt 0 ]]; do
        local doip_ver=$((0x$(get_byte "$hex_str" -1 1)))
        local doip_ver_rev=$((0x$(get_byte "$hex_str" -2 1)))

        if [ $doip_ver -ne $DOIP_PROTOCOL_VERSION ] || [ $doip_ver_rev -ne $calc_ver_rev ]; then
            rt=255
            G_VAR_MSG_DOIP_PARSE_LEN=$((G_VAR_MSG_DOIP_PARSE_LEN + 2))
            hex_str="${hex_str:2}"
            print_warning "ignore invalid msg:${doip_ver}\n"
            continue
        fi
        local payload_type
        local payload_len
        local frame_len
        payload_type="0x$(get_byte "$hex_str" -3 2)"
        payload_len=$((0x$(get_byte "$hex_str" -5 4)))
        frame_len=$((2 * (DOIP_HEADER_SIZE + payload_len)))

        print_debug "frame bytes:$((frame_len / 2)), msg:${hex_str:0:$frame_len}"
        print_debug "payload bytes:$payload_len, payload type:$payload_type"

        if [[ ${#hex_str} -lt $frame_len ]]; then
            # msg not enough
            rt=0
            print_warning "parse msg not enough\n"
            break
        else
            if [ -n "${DOIP_MSG_RESP[$payload_type]}" ]; then
                ${DOIP_MSG_RESP[$payload_type]} "${hex_str:$((DOIP_HEADER_SIZE * 2)):$((payload_len * 2))}"
                rt=$?
            else
                print_warning "not impliment doip function, payload type: $payload_type\n"
            fi

            if [ $rt -ne 0 ]; then
                break
            fi

            G_VAR_MSG_DOIP_PARSE_LEN=$((G_VAR_MSG_DOIP_PARSE_LEN + frame_len))
            hex_str="${hex_str:$frame_len}"
        fi
    done

    return $rt
}

function msg_doip_recv() {
    local bytes_num=0
    local str_bytes=""
    local rt=0
    local try_cnt=0
    local max_try_cnt=2
    local parsed_len=0
    local parsed_bytes=0

    # echo -ne "" >"$FILE_SOCK_RECV_RES"

    while [[ $rt -eq 0 ]]; do
        sock_tcp_recv "$FILE_SOCK_RECV_RES"
        rt=$?
        if [ $rt -ne 124 ] && [ $rt -ne 0 ]; then
            print_error "msg_doip_recv rt: $rt, res: $(cat "$FILE_SOCK_RECV_RES")\n"
            break
        fi
        rt=0
        str_bytes=$(dd if="$FILE_SOCK_RECV_RES" skip="$parsed_bytes" iflag=skip_bytes 2>/dev/null | hex_to_str)
        bytes_num=$((${#str_bytes} / 2))
        #bytes_num=$(($(wc -c "$FILE_SOCK_RECV_RES" | awk '{print $1}')))

        if [ $try_cnt -ge 1 ]; then
            print_warning "msg_doip_recv (try:$try_cnt/$max_try_cnt) bytes_num: $bytes_num\n"
        else
            print_debug "msg_doip_recv (try:$try_cnt/$max_try_cnt) bytes_num: $bytes_num"
        fi

        if [ ${bytes_num} -ge $DOIP_HEADER_SIZE ]; then
            #str_bytes=$(cat "$FILE_SOCK_RECV_RES" | hex_to_str)
            #bytes_num=$((${#str_bytes} / 2))
            msg_doip_parse "${str_bytes}"
            rt=$?
            parsed_len=$G_VAR_MSG_DOIP_PARSE_LEN

            if [ $rt -eq 255 ];then                       # error
                rt=255
                break
            elif [[ $parsed_len -eq ${#str_bytes} ]]; then # parse over
                break
            else                                           # parse partial
                parsed_bytes=$((parsed_bytes + parsed_len/2))
            fi
        fi

        try_cnt=$((try_cnt + 1))

        if [ $try_cnt -ge $max_try_cnt ]; then
            print_warning "msg_doip_recv break try_cnt:($try_cnt/$max_try_cnt), recved bytes:$bytes_num, msg:$str_bytes\n"
            rt=255
            break
        else
            print_warning "msg_doip_recv continue try_cnt:$try_cnt/$max_try_cnt, recved bytes:$bytes_num, msg:$str_bytes\n"
        fi
        sleep 0.5
    done
    
    return $rt
}

function msg_doip_send() {

    echo -ne "" >"$FILE_RECV_DIAG_MSG_RES"

    local msg="$1"
    local recvResp=$2
    recvResp=${recvResp:-1}

    print_debug "send doip bytes:$((${#msg} / 2)), msg:${msg}"

    str_to_hex "$msg" >"$FILE_SOCK_SEND"

    if [ ! -s "$FILE_SOCK_SEND" ];then
        print_error "send content ($FILE_SOCK_SEND) can't empty !"
        return 255
    fi

    sock_tcp_send "$FILE_SOCK_SEND" "$FILE_SOCK_RECV_RES"

    local rt=$?
    local cnt_send=1
    local max_send=2

    while [[ $rt -ne 0 ]] && [[ $cnt_send -lt $max_send ]];do
        cnt_send=$((cnt_send+1))
        sock_tcp_connect 1
        rt=$?

        if [ $rt -eq 0 ];then
            print_warning "sock_tcp_connect reconnect($cnt_send/$max_send) ok !\n"
            sock_tcp_send "$FILE_SOCK_SEND" "$FILE_SOCK_RECV_RES"
            rt=$?

            if [ $rt -eq 0 ];then
                print_warning "sock_tcp_send resend($cnt_send/$max_send) ok !\n"
                break
            else
                print_warning "sock_tcp_send resend($cnt_send/$max_send) failed !\n"
                sleep 1
            fi
        else
            print_warning "sock_tcp_connect reconnect($cnt_send/$max_send) failed !\n"
            sleep 1
        fi
    done

    [[ $recvResp -eq 0 ]] && return $rt

    if [ $rt -eq 0 ]; then
        msg_doip_recv
        rt=$?

        if [ $rt -eq 0 ];then
            local last_recv
            local recv_cnt=0
            last_recv=$(tail -n1 "$FILE_RECV_DIAG_MSG_RES")

            # when last recv is 8002 then continue recv 8001 uds content
            while [[ x"$last_recv" == x"diag_msg_resp_8001 $DOIP_RESP_CODE_PENDING" ]] || [[ x"$last_recv" =~ x"diag_msg_resp_8002" ]]; do
                if [[ $recv_cnt -eq 1 ]];then
                    print_warning "continue last recved msg:\"$last_recv\"\n"
                    print_warning "continue recv msg diag_msg_resp_8001 ...\n"
                fi

                [[ x"$last_recv" == x"diag_msg_resp_8001 $DOIP_RESP_CODE_PENDING" ]] && sleep 1

                msg_doip_recv
                rt=$?
                [[ $rt -ne 0 ]] && break

                last_recv=$(tail -n1 "$FILE_RECV_DIAG_MSG_RES")

                if [[ $recv_cnt -eq 1 ]];then
                    print_warning "continue last recved msg:\"$last_recv\"\n"
                fi

                recv_cnt=$((recv_cnt + 1))

                #print_info "recv ret: $rt"
            done

            if [[ $recv_cnt -ge 2 ]];then
                print_warning "last recved msg:\"$last_recv\"\n"
                print_warning "continue recv msg diag_msg_resp_8001 recv_cnt($recv_cnt)\n"
            fi
        fi
    else
        print_warning "sock_tcp_send ret: $rt"
    fi

    return $rt
}

############################## doip response uds message ####################################

function uds_repsp_sess_ctl_10() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_repsp_sec_acc_27() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_write_data_2e() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_read_data_22() {
    uds="$*"
    diag_res="${uds}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_routine_ctl_31() {
    uds="$*"
    diag_res="${uds}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_dtc_setting_85() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_comm_ctl_28() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_file_transfer_38() {
    uds="$*"
    diag_res="${uds:4:8}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_transfer_data_36() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

function uds_transfer_exit_37() {
    uds="$*"
    diag_res="${uds:2}"
    print_info "${FUNCNAME[0]} uds:${uds}, diag_res:${diag_res}"
    echo "${FUNCNAME[0]} $diag_res" >>"$FILE_RECV_DIAG_MSG_RES"
}

################################# uds protocol define ###################################

UDS_PROTO_SESSION_CTL=10
UDS_PROTO_SESSION_CTL_DEF=01
UDS_PROTO_SESSION_CTL_PROG=02
UDS_PROTO_SESSION_CTL_EXTEN=03

UDS_PROTO_SEC_ACC=27

UDS_PROTO_EXTEN_SEC_ACC_SEED=01 #19
UDS_PROTO_EXTEN_SEC_ACC_KEY=02  #1a
UDS_PROTO_PROG_SEC_ACC_SEED=01  #29
UDS_PROTO_PROG_SEC_ACC_KEY=02   #2a

UDS_PROTO_READ_DATA=22
UDS_PROTO_READ_DATA_PROG_STATUS=f1db # install status
UDS_PROTO_READ_DATA_INST_PROGRESS=f1da # install progress
UDS_PROTO_READ_DATA_SW=f1bc            # software info
UDS_PROTO_READ_DATA_HW=f1bd            # hardware info
# UDS_PROTO_READ_DATA_F18A=f18a
UDS_PROTO_READ_DATA_F170=f170
UDS_PROTO_READ_DATA_F171=f171

UDS_PROTO_WRITE_DATA=2e
UDS_PROTO_WRITE_DATA_FINGER_PRINT=f0ff

UDS_PROTO_ROUTINE_CTL=31
UDS_PROTO_ROUTINE_CTL_CHK_OTA_MODE_PRE_COND=010213  # CheckOTAModePreConditions (02h 13h)
UDS_PROTO_ROUTINE_CTL_PROG_PRE_COND=010203          # CheckProgrammingPreconditions(02h 02h)
UDS_PROTO_ROUTINE_CTL_CHK_PKG_MD5=010207            # CheckMD5 (02h 07h)
UDS_PROTO_ROUTINE_CTL_CHK_PROG_INTEGRITY=010211     # CheckProgrammingIntegrity (02h 11h)
UDS_PROTO_ROUTINE_CTL_CHK_PKG_SIG_VERIFI=010212     # PackageSignatureVerification (02h 12h)
UDS_PROTO_ROUTINE_CTL_START_INSTALL=010208          # install
UDS_PROTO_ROUTINE_CTL_START_ACTIVATE=010210         # activate partition

UDS_PROTO_DTC_SETTING=85
UDS_PROTO_DTC_SETTING_DISABLE_DTC=02 #82

UDS_PROTO_COMM_CTL=28
UDS_PROTO_COMM_CTL_ETH_SILENT=0301 #8301 eth silent

UDS_PROTO_FILE_TRANSFER=38      #transfer file
UDS_PROTO_FILE_TRANSFER_MODE=03 #replace mode

UDS_PROTO_TRANSFER_DATA=36 #data transfer
UDS_PROTO_TRANSFER_EXIT=37 #transfer exit

declare -A UDS_MSG_RESP=(
    [$UDS_PROTO_SESSION_CTL]=uds_repsp_sess_ctl_10
    [$UDS_PROTO_SEC_ACC]=uds_repsp_sec_acc_27
    [$UDS_PROTO_READ_DATA]=uds_read_data_22
    [$UDS_PROTO_WRITE_DATA]=uds_write_data_2e
    [$UDS_PROTO_ROUTINE_CTL]=uds_routine_ctl_31
    [$UDS_PROTO_DTC_SETTING]=uds_dtc_setting_85
    [$UDS_PROTO_COMM_CTL]=uds_comm_ctl_28
    [$UDS_PROTO_FILE_TRANSFER]=uds_file_transfer_38
    [$UDS_PROTO_TRANSFER_DATA]=uds_transfer_data_36
    [$UDS_PROTO_TRANSFER_EXIT]=uds_transfer_exit_37
)

#################################### diagnostic message request function #######################################

function uds_calc_key_673() {
    # key, seed is 4 bytes
    
    [[ -z "$1" ]] && echo "" && return 1

    local seed1=$1
    local seed2=0
    local key1
    local key2
    local AppKeyConst=18E25AFB # 0x18E25AFB
    local i=0

    # echo "seed1: $seed1"
    local rSeed

    # revert seed's bytes

    for ((i=0; i < 4; ++i));do
        rSeed=${rSeed}$(get_byte "$seed1" "$i" 1)
    done

    rSeed=$((0x$rSeed))

    # calc seed2 by seed1

    for ((i=0; i < 16; i++));do
        seed2=$(( seed2 | ((rSeed & (1 << i)) << (31 - 2 * i)) ))
        seed2=$(( seed2 | ((rSeed & (0x80000000 >> i)) >> (31 - 2 * i)) ))
        # echo "i:$i, seed2: $(hex_calc $seed2)"
    done

    seed2=$(hex_calc $seed2)
    
    while [[ ${#seed2} -lt 8 ]];do
        seed2=00${seed2}
    done

    rSeed=''

    # echo "seed2: $seed2"

    # revert seed's bytes

    for ((i=0; i < 4; ++i));do
        rSeed=${rSeed}$(get_byte "$seed2" "$i" 1)
    done

    seed2=$rSeed

    # echo "seed2: $seed2, seed1: $seed1"

    # calc key1, key2 by seed1, seed2

    for ((i=0; i < 4; i++));do
        local byte=$(( 0x$(get_byte "$seed1" "$i" 1) ^ 0x$(get_byte "$AppKeyConst" "$i" 1)))
        key1="$(hex_calc "$byte")$key1"

        byte=$((0x$(get_byte "$seed2" "$i" 1) ^ 0x$(get_byte "$AppKeyConst" "$i" 1)))
        key2="$(hex_calc "$byte")$key2"
    done

    # echo "key1: $key1, key2: $key2"
    local key
    key=$(hex_calc "0x$key1 + 0x$key2")
    key=$(get_byte "$key" 3 4)

    echo "$key"
}

function diag_msg_sess_ctrl_def() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_SESSION_CTL}${UDS_PROTO_SESSION_CTL_DEF})"
    local rt=$?
    return $rt
}

function diag_msg_sess_ctrl_exten() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_SESSION_CTL}${UDS_PROTO_SESSION_CTL_EXTEN})"
    local rt=$?
    return $rt
}

function read_data_by_id_hw_sw_info() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_READ_DATA}${UDS_PROTO_READ_DATA_HW})"
    local rt=$?
    local hw
    hw=$(sed -n -r "s/uds_read_data_22 (*)/\1/p" "$FILE_RECV_DIAG_MSG_RES")
    print_info "hardware info:$hw\n"

    if [ $rt -eq 0 ];then
        msg_doip_send "$(diag_msg_head ${UDS_PROTO_READ_DATA}${UDS_PROTO_READ_DATA_SW})"
        rt=$?
        local sw
        sw=$(sed -n -r "s/uds_read_data_22 (*)/\1/p" "$FILE_RECV_DIAG_MSG_RES")
        print_info "software info:$sw\n"

    fi

    return $rt
}

function write_data_by_id_finger_print() {
    print_info "call ${FUNCNAME[0]}"
    local finger
    finger="$(date '+%02y%0m%0d%H%M%S')"
    finger="${finger}$(printf "%.sf" {1..84})"
    msg_doip_send "$(diag_msg_head "${UDS_PROTO_WRITE_DATA}${UDS_PROTO_WRITE_DATA_FINGER_PRINT}${finger}")"
    local rt
    rt=$?
    return $rt
}

function sess_exten_diag_msg_sec_acc() {
    print_info "call ${FUNCNAME[0]}"

    msg_doip_send "$(diag_msg_head ${UDS_PROTO_SEC_ACC}${UDS_PROTO_EXTEN_SEC_ACC_SEED})"
    local rt=$?
    local seed
    seed=$(sed -n -r "s/uds_repsp_sec_acc_27 ([0-9a-eA-E]{8})*/\1/p" "$FILE_RECV_DIAG_MSG_RES")
    #local key="01020304"
    local key
    key=$(uds_calc_key_673 "$seed")
    print_info "seed:${seed}, key:${key}\n"
    [[ $rt -ne 0 ]] && return $rt
    msg_doip_send "$(diag_msg_head "${UDS_PROTO_SEC_ACC}${UDS_PROTO_EXTEN_SEC_ACC_KEY}${key}")"
    rt=$?

    return $rt
}

function routine_ctrl_chk_ota_mode_pre_cond() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_CHK_OTA_MODE_PRE_COND})"
    local rt=$?
    return $rt
}

function routine_ctrl_prog_pre_cond() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_PROG_PRE_COND})"
    local rt=$?
    return $rt
}

function routine_ctrl_chk_prog_integ() {
    print_info "call ${FUNCNAME[0]}\n"
    local md5_sum="$1"
    local rt=255
    msg_doip_send "$(diag_msg_head "${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_CHK_PROG_INTEGRITY}${md5_sum}")"
    rt=$?

    if [ $rt -eq 0 ];then
        local res
        res=$(tail -n1 "$FILE_RECV_DIAG_MSG_RES" | awk '{print $2}')
        print_info "check program itegrity (uds): $res\n"

        if [ x"$res" != x"${UDS_PROTO_ROUTINE_CTL_CHK_PROG_INTEGRITY}00" ];then
            print_warning "check program itegrity (md5:$md5_sum) incorrect result !\n"
            rt=255
        fi
    fi

    return $rt
}

function routine_ctrl_chk_pkg_sig_verif() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_CHK_PKG_SIG_VERIFI})"
    local rt=$?
}

function dtc_setting() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_DTC_SETTING}${UDS_PROTO_DTC_SETTING_DISABLE_DTC})"
    local rt=$?
    return $rt
}

function comm_ctl {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_COMM_CTL}${UDS_PROTO_COMM_CTL_ETH_SILENT})"
    local rt=$?
    return $rt
}

function diag_msg_sess_ctrl_prog() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head ${UDS_PROTO_SESSION_CTL}${UDS_PROTO_SESSION_CTL_PROG})"
    local rt=$?
    return $rt
}

function sess_prog_diag_msg_sec_acc() {
    print_info "call ${FUNCNAME[0]}"

    msg_doip_send "$(diag_msg_head ${UDS_PROTO_SEC_ACC}${UDS_PROTO_PROG_SEC_ACC_SEED})"
    local rt=$?
    local seed
    local key
    seed=$(sed -n -r "s/uds_repsp_sec_acc_27 ([0-9a-eA-E]{8})*/\1/p" "$FILE_RECV_DIAG_MSG_RES")
    #local key="01020304"
    key=$(uds_calc_key_673 "$seed")
    print_info "seed:${seed}, key:${key}\n"
    [[ $rt -ne 0 ]] && return $rt

    msg_doip_send "$(diag_msg_head "${UDS_PROTO_SEC_ACC}${UDS_PROTO_PROG_SEC_ACC_KEY}${key}")"
    rt=$?

    return $rt
}

function sess_prog_read_data_by_id() {
    print_info "call ${FUNCNAME[0]}"
    msg_doip_send "$(diag_msg_head "${UDS_PROTO_READ_DATA}${UDS_PROTO_READ_DATA_F170}")"
    local rt=$?

    if [ $rt -eq 0 ]; then
        msg_doip_send "$(diag_msg_head "${UDS_PROTO_READ_DATA}${UDS_PROTO_READ_DATA_F171}")"
        rt=$?
    fi

    return $rt
}

function read_data_by_id_prog_status() {
    print_info "call ${FUNCNAME[0]}"

    local status=''
    local rt=0

    echo -ne "" > "$FILE_OTA_PROG_STATUS"
    msg_doip_send "$(diag_msg_head "${UDS_PROTO_READ_DATA}${UDS_PROTO_READ_DATA_PROG_STATUS}")"
    rt=$?

    if [ $rt -eq 0 ];then
        status=$(sed -n -r "s/uds_read_data_22 ${UDS_PROTO_READ_DATA_PROG_STATUS}([0-9a-eA-E]{2})*/\1/p" "$FILE_RECV_DIAG_MSG_RES")
        rt=$?
    fi

    if [ $rt -ne 0 ];then
        print_error "error: $rt !\n"
    else
        print_info "OTA_PROG_STATUS:$status, ${OTA_PROG_STATUS[$status]}\n"
        echo -ne "OTA_PROG_STATUS:$status" > "$FILE_OTA_PROG_STATUS"
    fi

    return $rt
}

# 0: status not in array param
# 1 ~ count: array param's index (from 1)
# 255: error

function chk_ota_status() {
    print_info "call ${FUNCNAME[0]}"
    local chk_stat=($(echo "$@" | xargs)) # convert to array
    read_data_by_id_prog_status
    local rt=$?
    local i=0
    local cnt=${#chk_stat[@]}
    local err=255
    [[ $rt -ne 0 ]] && print_info "chk_ota_status ret: $err\n" && return $err
    local ota_status
    ota_status=$(sed -n -r "s/OTA_PROG_STATUS:([0-9a-eA-E]{2})*/\1/p" "$FILE_OTA_PROG_STATUS")
    rt=$?
    [[ $rt -ne 0 || -z "$ota_status" ]] && print_info "chk_ota_status ret: $err\n" && return $err

    #OTA_PROG_STAT_INIT=00
    #OTA_PROG_STAT_VERIFY=01
    #OTA_PROG_STAT_VERIFY_FAIL=02
    #OTA_PROG_STAT_VERIFY_SUCCESS=03
    #OTA_PROG_STAT_INSTALL=04
    #OTA_PROG_STAT_INSTALL_SUCCESS=05
    #OTA_PROG_STAT_INSTALL_FAIL=06
    #OTA_PROG_STAT_ACTIVATE=07
    #OTA_PROG_STAT_ACTIVATE_SUCCESS=08
    #OTA_PROG_STAT_ACTIVATE_FAIL=09
    #OTA_PROG_STAT_END=FF

    for ((i=0; i < cnt; ++i)) do
        [[ x"${chk_stat[$i]}" == x"$ota_status" ]] && print_info "chk_ota_status:$ota_status, ret:$((i+1))\n" && return $((i+1))
    done

    [[ x"$ota_status" == x"$OTA_PROG_STAT_INSTALL_FAIL"  \
    || x"$ota_status" == x"$OTA_PROG_STAT_ACTIVATE_FAIL"  \
    || x"$ota_status" == x"$OTA_PROG_STAT_VERIFY_FAIL" ]] && print_info "chk_ota_status:$ota_status, ret: $err\n" && return $err

    return 0
}

function read_data_by_id_inst_progress() {
    print_info "call ${FUNCNAME[0]}"
    local progress=0
    local progress_new=0
    local seconds
    local sec=0
    local max_cnt=240
    seconds=$(date +%s)
    chk_ota_status "$OTA_PROG_STAT_INSTALL"
    local rt=$?
    [[ $rt -eq 0 ]] && return 0   # step over
    [[ $rt -eq 255 ]] && return $rt # error

    while [[ 0x$progress -lt 100 ]];do
        msg_doip_send "$(diag_msg_head ${UDS_PROTO_READ_DATA}${UDS_PROTO_READ_DATA_INST_PROGRESS})"
        local rt=$?
        sec=$(($(date +%s) - seconds))

        if [ $rt -ne 0 ];then
            local sck_err
            sck_err=$(tail -n1 "$FILE_SOCK_ERR" | tr -d "\n")

            if [ -n "$sck_err" ];then
                if [[ $sck_err =~ "Connection timed out" ]] || [[ $sck_err =~ "No route to host" ]];then
                    print_warning "system maybe rebooting, try($sec/$max_cnt) ... \n"
                elif [[ $sck_err =~ "Connection refused" ]];then
                    print_warning "wait for doip service start, try($sec/$max_cnt) ... \n"
                elif [[ $sck_err =~ "write error: Broken pipe" ]];then
                    print_warning "socket is disconnect, try($sec/$max_cnt) ... \n"
                else
                    print_warning "$sck_err, try($sec/$max_cnt) ... \n"
                fi
            else
                local recved
                recved=$(tail -n1 "$FILE_RECV_DIAG_MSG_RES")

                if [[ $recved =~ "diag_msg_resp_8003" ]];then
                    print_error "recved diag_msg_resp_8003\n"
                    #routing_acti_req
                    break
                elif [[ $recved =~ "diag_msg_resp_8001 7f2210" ]];then
                    chk_ota_status "$OTA_PROG_STAT_INSTALL"
                    rt=255
                    break
                fi
            fi
        fi

        progress_new=$(sed -n -r "s/uds_read_data_22 ${UDS_PROTO_READ_DATA_INST_PROGRESS}([0-9a-eA-E]{2})*/\1/p" "$FILE_RECV_DIAG_MSG_RES")
        rt=$?

        if [ $rt -ne 0 ];then
            print_error "installing progress file error: $rt\n"
            break
        fi

        if [[ 0x$progress_new -eq 0x$progress ]];then
            if [ $sec -ge $max_cnt ];then
                print_error "installing progress($(( 0x$progress ))%) error timeout(>=$max_cnt) !\n"
                rt=255
                break
            fi
        else
            progress=$progress_new
            seconds=$(date +%s)
        fi

        print_info "installing(${sec}/${max_cnt}) progress($(( 0x$progress ))%)"
        sleep 1
    done

    print_info "installing progress($(( 0x$progress ))%)\n"

    if [ $rt -ne 0 ];then
        print_error "program install error !\n"
    else
        print_success "program install ok.\n"
    fi

    while [[ $rt -eq 0 ]];do
        chk_ota_status "$OTA_PROG_STAT_INSTALL $OTA_PROG_STAT_INSTALL_SUCCESS $OTA_PROG_STAT_INSTALL_FAIL"
        local ret=$?
        if [ $ret -eq 1 ];then
            sleep 1
        else 
            break
        fi
    done

    return $rt
}

function download_file() {
    print_info "call ${FUNCNAME[0]}"
    print_warning "not impliment !\n"
}

function transfer_file() {
    print_info "call ${FUNCNAME[0]}"
    local local_file="$1"
    local remote_file="$2"
    local file_type="$3"
    local rt=255
    local file_size
    file_size=$(
        wc -c "$local_file" | awk '{print $1}'
        exit "${PIPESTATUS[0]}"
    )

    if [[ $file_size -le 0 ]]; then
        print_error "file size ($((file_size))) error, please select correct file !\n"
        return 255
    fi

    print_info "local: $local_file, remote: $remote_file\n"

    if [ -n "$file_type" ];then
        local md5_sum
        md5_sum=$(md5sum "$local_file" | awk '{print $1}')
        print_info "file: $local_file, md5: $md5_sum, type: $file_type\n"
        msg_doip_send "$(diag_msg_head "${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_CHK_PKG_MD5}${DOIP_PROTOCOL_TA_ECU_2_BYTES}${file_type}0001${md5_sum}")"
        rt=$?
        [[ $rt -ne 0 ]] && return $rt
        local file_exist
        file_exist=$(tail -n1 "$FILE_RECV_DIAG_MSG_RES" | awk '{print $2}')
        print_info "file_exist(uds): $file_exist\n"
        [[ x"$file_exist" == x"${UDS_PROTO_ROUTINE_CTL_CHK_PKG_MD5}00" ]] && print_info "file already exist, ignore upload !" && return 0 # already exist
    fi
    local size_file_name
    size_file_name=$(hex_calc "$((${#remote_file} + 1))")

    # 2 bytes
    while [[ ${#size_file_name} -lt 4 ]];do
        size_file_name="00${size_file_name}"
    done
    size_file_name=$(get_byte "$size_file_name" 1 2)

    remote_file="$(hex_to_str "$remote_file")00"              # convert to hex string with null zero
    local enc_method=00                                     # compressionMethod, encryptingMethod dataFormatIdentifier
    local size_len=04                                       # fileSizeParameterLength
    local size_uncomp
    size_uncomp=$(hex_calc "$file_size")                # fileSizeUncompressed

    # 4 bytes
    while [[ ${#size_uncomp} -lt 8 ]];do
        size_uncomp="00${size_uncomp}"
    done

    size_uncomp=$(get_byte "$size_uncomp" 3 4)

    local size_compr=$size_uncomp                           # fileSizeCompressed

    msg_doip_send "$(diag_msg_head "${UDS_PROTO_FILE_TRANSFER}${UDS_PROTO_FILE_TRANSFER_MODE}${size_file_name}${remote_file}${enc_method}${size_len}${size_uncomp}${size_compr}")"
    rt=$?
    [[ $rt -ne 0 ]] && return $rt
    local block_bytes
    block_bytes=$((0x$(tail -n1 "$FILE_RECV_DIAG_MSG_RES" | awk '{print $2}')))
    local block_idx=0
    local read_size=0
    local percent=0

    if [ $block_bytes -le 0 ];then
        block_bytes=40960
    else
        block_bytes=$((block_bytes - 2))
    fi

    print_info "transfer data, block size:$block_bytes\n"

    while [[ $read_size -lt $file_size ]]; do
        local data
        data=$(dd count="$block_bytes" skip="$read_size" iflag=count_bytes,skip_bytes if="$local_file" 2>/dev/null | hex_to_str)
        rt=$?

        if [ -n "$data" ] && [ $rt -eq 0 ]; then

            block_idx=$((block_idx + 1))

            if [ $block_idx -gt 255 ];then
                block_idx=0
            fi

            msg_doip_send "$(diag_msg_head "${UDS_PROTO_TRANSFER_DATA}$(hex_calc "$block_idx" | get_byte)${data}")"
            rt=$?
            if [ $rt -ne 0 ];then
                print_warning "uploading read file break, rt:$rt, progress($percent%, $read_size/$file_size)\n"
                break
            fi
            #sleep 0.5
            read_size=$((read_size + $((${#data} / 2))))
            percent=$(echo "scale=1;100*$read_size/$file_size" | bc)
            percent=$(printf "%.1f" "$percent")
            print_info "uploading progress(i:$block_idx, $percent%, $read_size/$file_size) ..."
        else
            print_warning "uploading read file break, rt:$rt, progress($percent%, $read_size/$file_size)\n"
            break
        fi
    done

    if [ $rt -eq 0 ]; then
        print_success "uploading progress(i:$block_idx, $percent%, $read_size/$file_size)\n"
        msg_doip_send "$(diag_msg_head ${UDS_PROTO_TRANSFER_EXIT})"
        rt=$?
        
        if [ $rt -ne 0 ];then
            print_error "uploading send UDS_PROTO_TRANSFER_EXIT error !"
        else
            # routine_ctrl_chk_prog_integ $md5_sum
            # rt=$?
            rt=$rt
        fi
    else
        print_error "uploading progress:(i:$block_idx, ${percent}%, $read_size/$file_size), rt: $rt\n"
    fi

    return $rt
}

function transfer_file_sig() {
    print_info "call ${FUNCNAME[0]}"
    local file="${TMP_PATH}/swdl.tar.sig"
    cp "${UPGRADE_TAR_FILE}.sig" "$file"
    transfer_file "$file" "/swdl/doip/swdl.tar.sig" # "01"
    local rt=$?
    rm "$file"
    ### routine_ctrl_chk_pkg_sig_verif

    return $rt
}

function transfer_file_pkg() {
    print_info "call ${FUNCNAME[0]}"

    chk_ota_status "$OTA_PROG_STAT_INIT $OTA_PROG_STAT_ACTIVATE_SUCCESS $OTA_PROG_STAT_END $OTA_PROG_STAT_INSTALL_FAIL"
    local rt=$?
    [[ $rt -eq 0 ]] && return 0   # step over
    [[ $rt -eq 255 ]] && return $rt # error

    local file="${TMP_PATH}/swdl.tar.zip"
    cp "${UPGRADE_TAR_FILE}" "$file"
    transfer_file "$file" "/swdl/doip/swdl.tar.zip" # "02"
    #scp swdl_tar/swdl*.zip root@192.168.31.123:/swdl/doip/swdl.tar.zip
    rt=$?
    rm "$file"

    #if [ $rt -eq 0 ];then
    #    print_warning "installing will auto start later ...\n"
    #    sleep 2
    #fi

    return $rt
}

function routine_ctrl_start_install() {
    print_info "call ${FUNCNAME[0]}"

    chk_ota_status "$OTA_PROG_STAT_INIT $OTA_PROG_STAT_ACTIVATE_SUCCESS $OTA_PROG_STAT_END $OTA_PROG_STAT_INSTALL_FAIL"
    local rt=$?
    [[ $rt -eq 0 ]] && return 0   # step over
    [[ $rt -eq 255 ]] && return $rt # error

    msg_doip_send "$(diag_msg_head ${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_START_INSTALL})"
    rt=$?
    [[ $rt -ne 0 ]] && return $rt

    local result
    result=$(sed -n -r "s/uds_routine_ctl_31 ${UDS_PROTO_ROUTINE_CTL_START_INSTALL}([0-9a-eA-E]{2})*/\1/p" "$FILE_RECV_DIAG_MSG_RES")

    #print_warning "$(cat $FILE_RECV_DIAG_MSG_RES)\n"

    if [[ "$result" != "00" ]];then
        print_warning "start install failed, code: $result !\n"
        rt=1
    fi

    while [[ $rt -eq 0 ]];do
        chk_ota_status "$OTA_PROG_STAT_INIT $OTA_PROG_STAT_VERIFY $OTA_PROG_STAT_VERIFY_SUCCESS $OTA_PROG_STAT_INSTALL $OTA_PROG_STAT_INSTALL_SUCCESS $OTA_PROG_STAT_INSTALL_FAIL"
        local ret=$?
        if [[ $ret -eq 1 || $ret -eq 2 || $ret -eq 3 ]];then
            sleep 1
        else
            break
        fi
    done

    return $rt
}

function routine_ctrl_start_activate() {
    print_info "call ${FUNCNAME[0]}"

    chk_ota_status "$OTA_PROG_STAT_INSTALL_SUCCESS"
    local rt=$?
    [[ $rt -eq 0 ]] && return 0   # step over
    [[ $rt -eq 255 ]] && return $rt # error

    msg_doip_send "$(diag_msg_head ${UDS_PROTO_ROUTINE_CTL}${UDS_PROTO_ROUTINE_CTL_START_ACTIVATE})"
    rt=$?
    [[ $rt -ne 0 ]] && return $rt
    local result
    result=$(sed -n -r "s/uds_routine_ctl_31 ${UDS_PROTO_ROUTINE_CTL_START_ACTIVATE}([0-9a-eA-E]{2})*/\1/p" "$FILE_RECV_DIAG_MSG_RES")

    #print_warning "$(cat $FILE_RECV_DIAG_MSG_RES)\n"

    if [[ "$result" != "00" ]];then
        print_warning "start activate failed, code: $result !\n"
        rt=1
    fi

    local i=1
    local max=30
    local ping_failed=0

    while [[ $rt -eq 0 && $i -le $max ]];do
        local ret=0
        print_info "waiting for device reboot ($i/$max) ret:$ret ...\n"
        i=$((i+1))
        ping_test $LINENO 1
        ret=$?
        [[ $ping_failed -eq 0 ]] && ping_failed=$ret
        [[ $ret -eq 0 && $ping_failed -ne 0 ]] && print_info "ping ok !\n" && break
        sleep 1
    done

    #print_info "waiting for reset ($i/$max) ...\n"

    if [ $rt -eq 0 ];then
        sleep 5
        print_info "retry connecting ... \n"
        sock_tcp_close
        sock_tcp_connect 10
        rt=$?
    fi

    if [ $rt -eq 0 ];then
        sleep 5
        session_ctrl
        rt=$?
    fi

    while [[ $rt -eq 0 ]];do
        chk_ota_status "$OTA_PROG_STAT_INIT $OTA_PROG_STAT_ACTIVATE $OTA_PROG_STAT_ACTIVATE_SUCCESS"
        local ret=$?
        #[[ $ret -eq 0 ]] && break                   # step over
        [[ $ret -eq 1 || $ret -eq 2 ]] && sleep 1   # activating
        [[ $ret -eq 3 ]] && print_success "program activate success\n" && break    # activate success
        [[ $ret -eq 255 ]] && break                 # error
    done

    return $rt
}

function session_ctrl() {
    local rt=0
    local k=0

    for k in ${!DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL[*]}; do
        print_info "START_SESSION_CTRL step[$((k + 1))/${#DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL[@]}] ${DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL[$k]}\n"
        ${DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL[$k]}
        rt=$?
        print_info "END_SESSION_CTRL step[$((k + 1))/${#DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL[@]}] ${DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL[$k]}\n"
        [[ $rt -ne 0 ]] && break
    done

    return $rt
}

OTA_PROG_STAT_INIT=00
OTA_PROG_STAT_VERIFY=01
OTA_PROG_STAT_VERIFY_FAIL=02
OTA_PROG_STAT_VERIFY_SUCCESS=03
OTA_PROG_STAT_INSTALL=04
OTA_PROG_STAT_INSTALL_SUCCESS=05
OTA_PROG_STAT_INSTALL_FAIL=06
OTA_PROG_STAT_ACTIVATE=07
OTA_PROG_STAT_ACTIVATE_SUCCESS=08
OTA_PROG_STAT_ACTIVATE_FAIL=09
OTA_PROG_STAT_END=FF

declare -A OTA_PROG_STATUS=(
    [$OTA_PROG_STAT_INIT]="OTA_PROG_STAT_INIT"
    [$OTA_PROG_STAT_VERIFY]="OTA_PROG_STAT_VERIFY"
    [$OTA_PROG_STAT_VERIFY_FAIL]="OTA_PROG_STAT_VERIFY_FAIL"
    [$OTA_PROG_STAT_VERIFY_SUCCESS]="OTA_PROG_STAT_VERIFY_SUCCESS"
    [$OTA_PROG_STAT_INSTALL]="OTA_PROG_STAT_INSTALL"
    [$OTA_PROG_STAT_INSTALL_SUCCESS]="OTA_PROG_STAT_INSTALL_SUCCESS"
    [$OTA_PROG_STAT_INSTALL_FAIL]="OTA_PROG_STAT_INSTALL_FAIL"
    [$OTA_PROG_STAT_ACTIVATE]="OTA_PROG_STAT_ACTIVATE"
    [$OTA_PROG_STAT_ACTIVATE_SUCCESS]="OTA_PROG_STAT_ACTIVATE_SUCCESS"
    [$OTA_PROG_STAT_ACTIVATE_FAIL]="OTA_PROG_STAT_ACTIVATE_FAIL"
    [$OTA_PROG_STAT_END]="OTA_PROG_STAT_END"
)

DOIP_MSG_REQ_SEQUENCE_SESSION_CTRL=(
    routing_acti_req
    diag_msg_sess_ctrl_def
    diag_msg_sess_ctrl_exten

    ### routine_ctrl_chk_ota_mode_pre_cond
    ### read_data_by_id_hw_sw_info

    sess_exten_diag_msg_sec_acc

    routine_ctrl_prog_pre_cond
    dtc_setting
    comm_ctl
    diag_msg_sess_ctrl_prog
    sess_prog_diag_msg_sec_acc

    ### sess_prog_read_data_by_id
    # write_data_by_id_finger_print
)

DOIP_MSG_REQ_SEQUENCE=(
    session_ctrl

    ### transfer_file_sig
    transfer_file_pkg

    routine_ctrl_start_install
    read_data_by_id_inst_progress
    routine_ctrl_start_activate
    diag_msg_sess_ctrl_def
)

declare -A DOIP_MSG_RESP=(
    [$DOIP_PROTO_ALIVE_CHECK_REQ]=alive_check_resp
    [$DOIP_PROTO_ROUTING_ACTIVATION_RESP]=routing_acti_resp
    [$DOIP_PROTO_DIAGNOSTIC_REQ]=diag_msg_resp_8001
    [$DOIP_PROTO_DIAGNOSTIC_REQ_ACK]=diag_msg_resp_8002
    [$DOIP_PROTO_DIAGNOSTIC_REQ_NACK]=diag_msg_resp_8003
)

################################# main #########################################

function print_help()
{
cat <<EOF

You can specify one or more of the following parameters,
When running again after this, No need for the parameters again, 
Parameters will be automatically saved in this file.

--help: print this help info

-d: directory to find *.zip files,
    if find multiple files and total count (-c) greater than 1,
    program will use each file alternately 

-c: total count to upgrade

-h: IP of board's system

EOF
}

save_cfg()
{
	[[ x"$TAR_PATH" != x"$CFG_TAR_PATH" ]] && sed -i 's#^CFG_TAR_PATH=\".*\"$#CFG_TAR_PATH=\"'$TAR_PATH'\"#' $CUR_FILE
	[[ x"$IP" != x"$CFG_IP" ]] && sed -i 's#^CFG_IP=\".*\"$#CFG_IP=\"'$IP'\"#' $CUR_FILE
	[[ x"$TOTAL_CNT" != x"$CFG_TOTAL_CNT" ]] && sed -i 's#^CFG_TOTAL_CNT=[0-9]*$#CFG_TOTAL_CNT='$TOTAL_CNT'#' $CUR_FILE
}

function parse_args() {

    local params="$*"

    TAR_PATH=$(echo "$params" | sed -n -r -e "s#.*-d\s*(\S+).*#\1#p")
    IP=$(echo "$params" | sed -n -r -e "s#.*-h\s*([0-9]{2,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}).*#\1#p")
    TOTAL_CNT=$(echo "$params" | sed -n -r -e "s#.*-c\s*([0-9]+).*#\1#p")
    local HELP
    HELP=$(echo "$params" | sed -n -r -e "s#.*--(help).*#\1#p")

    while [[ "${TAR_PATH:-1}" == "/" ]]
    do
        TAR_PATH="${TAR_PATH%?}"
    done

    [[ ! -z "$params" ]] && [[ -z "$TAR_PATH" ]] && [[ -z "$IP" ]] && [[ -z "$TOTAL_CNT" ]] && HELP="--help"
    [[ ! -z "$HELP" ]] && print_help && exit 0

    [[ -z "$TAR_PATH" ]] && TAR_PATH=$CFG_TAR_PATH
    [[ -z "$IP" ]] && IP=$CFG_IP
    [[ -z "$TOTAL_CNT" ]] && TOTAL_CNT=$CFG_TOTAL_CNT

    save_cfg

    if [ -n "$TAR_PATH" ];then
        mkdir -p "$TAR_PATH"
    else
        print_error "tar path can't empty !\n"
    fi

    print_info ""
    print_info "Version: \"$VERSION\"\n"
    print_info "$>: doip_client.sh -d $TAR_PATH -c $TOTAL_CNT -h $IP\n"
    print_info ""
}

function start_program_once() {
    local cnt=$1
    local max_cnt=$2
    local rt=255
    print_info "begin program [$cnt/$max_cnt]\n"
    ping_test $LINENO 30
    local rt=$?
    [[ $rt -ne 0 ]] && return $rt

    print_info "connect start ...\n"
    sock_tcp_connect 30
    rt=$?
    [[ $rt -ne 0 ]] && return $rt

    for k in ${!DOIP_MSG_REQ_SEQUENCE[*]}; do
        print_info "BEGIN step[$((k + 1))/${#DOIP_MSG_REQ_SEQUENCE[@]}] ${DOIP_MSG_REQ_SEQUENCE[$k]}\n"
        ${DOIP_MSG_REQ_SEQUENCE[$k]}
        rt=$?
        print_info "END step[$((k + 1))/${#DOIP_MSG_REQ_SEQUENCE[@]}] ${DOIP_MSG_REQ_SEQUENCE[$k]}\n"
        [[ $rt -ne 0 ]] && break
    done

    sock_tcp_close
    print_info "end program [$cnt/$max_cnt].\n"
    return $rt
}

function find_tar_files() {
    local zip_format="zip"
    local i=1
    local files
    files="$(find $TAR_PATH -maxdepth 1 -name "*.$zip_format")"
    [[ ! -z $files ]] && files="$(echo "$files" | xargs ls -ltr | awk '{print $9}')"

    while read -r f
    do
        if [ -s "$f" ];then
            TAR_FILES[$i]="$f"
            print_info "tar_file ($i): ${TAR_FILES[$i]}\n"
            i=$((i+1))
        fi
    done <<< "$(echo -n "$files")"

    local size=${#TAR_FILES[@]}

    if [ "$size" -eq 0 ];then
        print_error "no such file \"$TAR_PATH/*.$zip_format\"\n"
        return 255
    fi

    local index=1
    local idx_read=""

    if [ "$size" -gt 1 ];then
        while [[ ! "$idx_read" =~ ^[0-9]{1,}$ ]] || [[ $idx_read -lt 1 ]] || [[ $idx_read -gt $size ]];do
            read -r -t 10 -p "Please select tar file (1 ~ $size, default: 1) : " idx_read
            [[ $? -eq 142 ]] && idx_read=1 && break
        done
        index=$idx_read
    fi

    UPGRADE_TAR_FILE="${TAR_FILES[$index]}"

    print_info ""
    print_info "Use file ($index) : $UPGRADE_TAR_FILE\n"

    return 0
}

function kill_pid_children() {
	local pid=$1
	#if [ -d /proc/$pid ];then
	#	kill -9 $pid >&2
	#fi
    [[ -z "$pid" ]] && return
    local pids
    pids=$(pstree -p "$pid" | sed -r -e 's/(.*)pstree\([0-9]+\)/\1/g' | grep -o "[0-9]*" | xargs)
	[[ ! -z "$pids" ]] && kill -9 "$pids" >&2
}

function init_env() {
    mkdir -p "$LOG_PATH"
    local pid=''

    if [ -s "$FILE_TOP_PID" ] && [ -d "/proc/$FILE_TOP_PID" ];then
        pid=$(cat "$FILE_TOP_PID")
        print_warning "[Line:$LINENO] prev running script (pid:$pid) will be killed !\n"
        kill_pid_children "$pid"
    fi

    if [ -s "$FILE_SOCK_RECV_PID" ];then
        pid=$(cat "$FILE_SOCK_RECV_PID")
        kill_pid_children "$pid" 2>/dev/null
    fi

    if [ -s "$FILE_SOCK_RECV_SIZE_PID" ];then
        pid=$(cat "$FILE_SOCK_RECV_SIZE_PID")
        kill_pid_children "$pid" 2>/dev/null
    fi

    echo -ne "" > "$FILE_SOCK_ERR"
    echo -ne "" > "$FILE_SOCK_SEND"
    echo -ne "" > "$FILE_SOCK_RECV_RES"
    echo -ne "" > "$FILE_RECV_DIAG_MSG_RES"
    echo -ne "" > "$FILE_LOG"
    echo -ne "" > "$FILE_SOCK_RECV_PID"
    echo -ne "" > "$FILE_SOCK_RECV_SIZE_PID"
    echo -ne "" > "$FILE_OTA_PROG_STATUS"
    echo -ne $TOP_SHELL_PID > "$FILE_TOP_PID"
}

function exist_sock_recv_size_proc() {
    local killed=$1
    local pidfile="$FILE_SOCK_RECV_SIZE_PID"
    local pid=''
    local fd=''
    local ppid=''

    if [ -s "$pidfile" ];then
        pid=$(cat "$pidfile")

        if [ -n "$pid" ];then
           fd=$( grep "Name:" 2>/dev/null < "/proc/$pid/status" | sed -n -r 's/Name: *(.*)/\1/p' | xargs )
           ppid=$( grep "PPid:" 2>/dev/null < "/proc/$pid/status" | sed -n -r 's/PPid: *(.*)/\1/p' | xargs )
        fi

        if [[ -n "$pid" && $killed -eq 1 ]];then
           print_warning "exist_sock_recv_size_proc killed, pid:$pid, fd:$fd\n"
            
            if [[ x"$fd" != x"" && $CUR_FILE =~ $fd ]];then
                echo -ne "" > "$pidfile" && kill_pid_children "$pid"
            elif [[ $ppid -eq $TOP_SHELL_PID ]];then
                echo -ne "" > "$pidfile" && kill_pid_children "$pid"
            fi

           echo -ne "" > "$FILE_SOCK_RECV_SIZE"
           fd=''
        fi
    fi

    if [[ $killed -eq 0 && -z "$pid" ]];then
        get_sock_recv_size & pid=$!; echo -ne $pid > "$pidfile"
        sync -f "$pidfile"
        print_warning "exist_sock_recv_size_proc create, pid:$pid, fd:$fd\n"
        fd=$pid
        sleep 0.1
    fi

    #print_warning "exist_sock_recv_size_proc pid:$pid, fd:$fd, kill:$killed\n"

    if [ -n "$fd" ];then
        return 0
    else
        return 1
    fi
} 

function exist_sock_recv_proc() {
    local killed=$1
    local pidfile="$FILE_SOCK_RECV_PID"
    local pid=''
    local fd=''
    local ppid=''

    if [ -s "$pidfile" ];then
        pid=$(cat "$pidfile")

        if [ -n "$pid" ];then
            fd=$( grep "Name:" 2>/dev/null < "/proc/$pid/status" | sed -n -r 's/Name: *(.*)/\1/p' | xargs )
            ppid=$( grep "PPid:" 2>/dev/null < "/proc/$pid/status" | sed -n -r 's/PPid: *(.*)/\1/p' | xargs )
        fi

        if [[ -n "$pid" && $killed -eq 1 ]];then
            print_warning "exist_sock_recv_proc killed, pid:$pid, fd:$fd\n"
            if [[ x"$fd" != x"" && $CUR_FILE =~ $fd ]];then
                echo -ne "" > "$pidfile" && kill_pid_children "$pid"
            elif [[ $ppid -eq $TOP_SHELL_PID ]];then
                echo -ne "" > "$pidfile" && kill_pid_children "$pid"
            fi

            #echo -ne "" > $pidfile && kill_pid_children $pid
            echo -ne "" > "$FILE_SOCK_RECV_RES"
            fd=''
        fi
    fi

    if [[ $killed -eq 0 && -z "$pid" ]];then
        get_sock_recv & pid=$!; echo -ne "$pid" > "$pidfile"
        sync -f "$pidfile"
        print_warning "exist_sock_recv_proc create, pid:$pid, fd:$fd\n"
        fd=$pid
        sleep 0.1
    fi

    #print_warning "exist_sock_recv_proc pid:$pid, fd:$fd, kill:$killed\n"

    if [ -n "$fd" ];then
        return 0
    else 
        return 1
    fi
}

function shell_exit() {
    exist_sock_recv_proc 1
    exist_sock_recv_size_proc 1
    print_info "shell_exit.\n\n"
    rm -rf "${LOG_PATH}/tmp_logs/"
    mkdir -p "${LOG_PATH}/tmp_logs/"
    cp -r "${TMP_PATH}"/file_* "${LOG_PATH}/tmp_logs/"
    wait
    sleep 1
    sudo umount "$TMP_PATH"
}

function main() {
    print_info "main start"
    trap "shell_exit" EXIT
    init_env
    parse_args "$*"

    local max_cnt=$TOTAL_CNT
    local success_cnt=0
    local failed_cnt=0
    local rt=0
    local i=1

    if [ ! -d "$LOG_PATH" ];then
        print_error "[Line:$LINENO] dir \"$LOG_PATH\" not exist !\n"
        print_info ""
        return 255
    fi

    find_tar_files
    rt=$?
    [[ $rt -ne 0 ]] && return 255

    for ((i=1; i <= max_cnt; i++));do
        print_info ""
        print_info "++++++ Loop begin[$i/$max_cnt] success: $success_cnt, failed: $failed_cnt ++++++ \n"
        start_program_once $i $max_cnt
        rt=$?
        [[ $rt -eq 0 ]] && success_cnt=$((success_cnt + 1)) || failed_cnt=$((failed_cnt + 1))
        print_info "------ Loop end[$i/$max_cnt] success: $success_cnt, failed: $failed_cnt ------ \n"
        print_info ""
    done

    print_info "main exit.\n\n"
    return $rt
}

main "$*"
