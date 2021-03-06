<?php
/*
	openbgpd.inc
	Copyright (C) 2007 Scott Ullrich (sullrich@gmail.com)
	part of pfSense
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
require_once("config.inc");
require_once("functions.inc");
require_once("service-utils.inc");

define('PKG_BGPD_CONFIG_BASE', '/var/etc/openbgpd');

$pf_version=substr(trim(file_get_contents("/etc/version")),0,3);
if ($pf_version > 2.0)
	define('PKG_BGPD_BIN', '/usr/pbi/openbgpd-' . php_uname("m") . '/sbin');
else
	define('PKG_BGPD_BIN','/usr/local/sbin');

define('PKG_BGPD_LOGIN',	"_bgpd");
define('PKG_BGPD_UID',		"130");
define('PKG_BGPD_GROUP',	"_bgpd");
define('PKG_BGPD_GID',		"130");
define('PKG_BGPD_GECOS',	"BGP Daemon");
define('PKG_BGPD_HOMEDIR',	"/var/empty");
define('PKG_BGPD_SHELL',	"/usr/sbin/nologin");

function openbgpd_install_conf() {
	global $config, $g;
	$pkg_login	= PKG_BGPD_LOGIN;
	$pkg_uid	= PKG_BGPD_UID;
	$pkg_group	= PKG_BGPD_GROUP;
	$pkg_gid	= PKG_BGPD_GID;
	$pkg_gecos	= PKG_BGPD_GECOS;
	$pkg_homedir	= PKG_BGPD_HOMEDIR;
	$pkg_shell	= PKG_BGPD_SHELL;
	$pkg_bin	= PKG_BGPD_BIN;

	conf_mount_rw();

	// Since we need to embed this in a string, copy to a var. Can't embed constnats.
	$bgpd_config_base = PKG_BGPD_CONFIG_BASE;
	if ($config['installedpackages']['openbgpd']['rawconfig'] && $config['installedpackages']['openbgpd']['rawconfig']['item']) {
		// if there is a raw config specified in the config.xml use that instead of the assisted config
		$conffile = implode("\n",$config['installedpackages']['openbgpd']['rawconfig']['item']);
		//$conffile = $config['installedpackages']['openbgpd']['rawconfig'];
	} else {
		// generate bgpd.conf based on the assistant
		if($config['installedpackages']['openbgpd']['config'])
			$openbgpd_conf = &$config['installedpackages']['openbgpd']['config'][0];
		if($config['installedpackages']['openbgpd']['config'][0]['row'])
			$openbgpd_rows = &$config['installedpackages']['openbgpd']['config'][0]['row'];
		if($config['installedpackages']['openbgpdgroups']['config'])
			$openbgpd_groups = &$config['installedpackages']['openbgpdgroups']['config'];
		if($config['installedpackages']['openbgpdneighbors']['config'])
			$openbgpd_neighbors = &$config['installedpackages']['openbgpdneighbors']['config'];

		$conffile = "# This file was created by the package manager.  Do not edit!\n\n";

		// Setup AS #
		if($openbgpd_conf['asnum'])
			$conffile .= "AS {$openbgpd_conf['asnum']}\n";

		if($openbgpd_conf['fibupdate'])
			$conffile .= "fib-update {$openbgpd_conf['fibupdate']}\n";

		// Setup holdtime if defined.  Default is 90.
		if($openbgpd_conf['holdtime'])
			$conffile .= "holdtime {$openbgpd_conf['holdtime']}\n";

		// Specify listen ip
		if(!empty($openbgpd_conf['listenip']))
			$conffile .= "listen on {$openbgpd_conf['listenip']}\n";
		else
			$conffile .= "listen on 0.0.0.0\n";
			
		// Specify router id
		if($openbgpd_conf['routerid'])
			$conffile .= "router-id {$openbgpd_conf['routerid']}\n";

		// Handle advertised networks
		if($config['installedpackages']['openbgpd']['config'][0]['row'])
			if(is_array($openbgpd_rows))
				foreach($openbgpd_rows as $row)
					$conffile .= "network {$row['networks']}\n";

		// Attach neighbors to their respective group owner
		if(is_array($openbgpd_groups)) {
			foreach($openbgpd_groups as $group) {
				$conffile .= "group \"{$group['name']}\" {\n";
				$conffile .= "	remote-as {$group['remoteas']}\n";
				if(is_array($openbgpd_neighbors)) {
					foreach($openbgpd_neighbors as $neighbor) {
						if($neighbor['groupname'] == $group['name']) {
							$conffile .= "\tneighbor {$neighbor['neighbor']} {\n";
							$conffile .= "\t\tdescr \"{$neighbor['descr']}\"\n";
							if($neighbor['md5sigpass']) {
								$conffile .= "\t\ttcp md5sig password {$neighbor['md5sigpass']}\n";
							}
							if($neighbor['md5sigkey']) {
								$conffile .= "\t\ttcp md5sig key {$neighbor['md5sigkey']}\n";
							}
							$setlocaladdr = true;
							if (is_array($neighbor['row'])) {
								foreach($neighbor['row'] as $row) {
									if ($row['parameters'] == "local-address")
										$setlocaladdr = false;
									$conffile .= "\t\t{$row['parameters']} {$row['parmvalue']} \n";
								}
							}
							if ($setlocaladdr == true) {
								if (!empty($openbgpd_conf['listenip'])) {
									$conffile .= "\t\tlocal-address {$openbgpd_conf['listenip']}\n";
								} else {
									$conffile .= "\t\tlocal-address 0.0.0.0\n";
								}
							}
								
							$conffile .= "}\n";
						}
					}
				}
				$conffile .= "}\n";
			}
		}

		// Handle neighbors that do not have a group assigned to them
		if(is_array($openbgpd_neighbors)) {
			foreach($openbgpd_neighbors as $neighbor) {
				if($neighbor['groupname'] == "") {
					$conffile .= "neighbor {$neighbor['neighbor']} {\n";
					$conffile .= "\tdescr \"{$neighbor['descr']}\"\n";
					if  ($neighbor['md5sigpass']) {
						$conffile .= "\ttcp md5sig password {$neighbor['md5sigpass']}\n";
					}
					if ($neighbor['md5sigkey']) {
						$conffile .= "\ttcp md5sig key {$neighbor['md5sigkey']}\n";
					}
					$setlocaladdr = true;
					if (is_array($neighbor['row'])) {
						foreach($neighbor['row'] as $row) {
							if ($row['parameters'] == "local-address")
								$setlocaladdr = false;
							$conffile .= "\t{$row['parameters']} {$row['parmvalue']} \n";
						}
					}
					if ($setlocaladdr == true && !empty($openbgpd_conf['listenip']))
						$conffile .= "\tlocal-address {$openbgpd_conf['listenip']}\n";
					else
						$conffile .= "\tlocal-address 0.0.0.0\n";

					$conffile .= "}\n";
				}
			}
		}

		// OpenBGPD filters
		$conffile .= "deny from any\n";
		$conffile .= "deny to any\n";
		if(is_array($openbgpd_neighbors)) {
			foreach($openbgpd_neighbors as $neighbor) {
				$conffile .= "allow from {$neighbor['neighbor']}\n";
				$conffile .= "allow to {$neighbor['neighbor']}\n";
			}
		}
	}
	safe_mkdir($bgpd_config_base);
	// Write out the configuration file
	@file_put_contents("{$bgpd_config_base}/bgpd.conf", $conffile);
	@chmod("{$bgpd_config_base}/bgpd.conf", 0600);
	unset($conffile);

	// Create rc.d file
	$rc_file_stop = <<<EOF
killall -TERM bgpd
EOF;
	$rc_file_start = <<<EOF

if [ `pw groupshow {$pkg_group} 2>&1 | grep -c "pw: unknown group"` -gt 0 ]; then
	/usr/sbin/pw groupadd {$pkg_group} -g {$pkg_gid}
fi
if [ `pw usershow {$pkg_login} 2>&1 | grep -c "pw: no such user"` -gt 0 ]; then
	/usr/sbin/pw useradd {$pkg_login} -u {$pkg_uid} -g {$pkg_gid} -c "{$pkg_gecos}" -d {$pkg_homedir} -s {$pkg_shell}
fi

/bin/mkdir -p {$bgpd_config_base}
/usr/sbin/chown -R root:wheel {$bgpd_config_base}
/bin/chmod 0600  {$bgpd_config_base}/bgpd.conf

NUMBGPD=`ps auxw | grep -c '[b]gpd.*parent'`
if [ \${NUMBGPD} -lt 1 ] ; then
	{$pkg_bin}/bgpd -f {$bgpd_config_base}/bgpd.conf
else
	{$pkg_bin}/bgpctl reload
fi
EOF;
	write_rcfile(array(
			"file" => "bgpd.sh",
			"start" => $rc_file_start,
			"stop" =>  $rc_file_stop
		)
	);
	unset($rc_file_start, $rc_file_stop);

	$_gb = exec("/sbin/sysctl net.inet.ip.ipsec_in_use=1");
	// bgpd process running?  if so reload, else start.
	if(is_openbgpd_running() == true) {
		exec("{$pkg_bin}/bgpctl reload");
	} else {
		exec("{$pkg_bin}/bgpd -f {$bgpd_config_base}/bgpd.conf");
	}

	conf_mount_ro();
}

// get the raw openbgpd confi file for manual inspection/editing
function openbgpd_get_raw_config() {
	$conf = PKG_BGPD_CONFIG_BASE . "/bgpd.conf";
	if (file_exists($conf))
		return file_get_contents($conf);
	else
		return "";
}

// serialize the raw openbgpd config file to config.xml
function openbgpd_put_raw_config($conffile) {
	global $config;
	if ($conffile == "")
		unset($config['installedpackages']['openbgpd']['rawconfig']);
	else {
		$config['installedpackages']['openbgpd']['rawconfig'] = array();
		$config['installedpackages']['openbgpd']['rawconfig']['item'] = explode("\n",$_POST['openbgpd_raw']);
		//$config['installedpackages']['openbgpd']['rawconfig'] = $conffile;
	}
}

function deinstall_openbgpd() {
	global $config, $g;

	exec("rm /usr/local/etc/rc.d/bgpd.sh");
	exec("rm /usr/local/www/openbgpd_status.php");
	exec("killall bgpd");
}

function check_group_usage($groupname) {
	global $config, $g;

	if($config['installedpackages']['openbgpd']['config'])
		$openbgpd_conf = &$config['installedpackages']['openbgpd']['config'][0];
	if($config['installedpackages']['openbgpd']['config'][0]['row'])
		$openbgpd_rows = &$config['installedpackages']['openbgpd']['config'][0]['row'];
	if($config['installedpackages']['openbgpdgroups']['config'])
		$openbgpd_groups = &$config['installedpackages']['openbgpdgroups']['config'];
	if($config['installedpackages']['openbgpdneighbors']['config'])
		$openbgpd_neighbors = &$config['installedpackages']['openbgpdneighbors']['config'];

	if(is_array($openbgpd_groups)) {
		foreach($openbgpd_groups as $group) {
			foreach($openbgpd_neighbors as $neighbor) {
				if($neighbor['groupname'] == $group['name'])
					return $neighbor['groupname'];
			}
		}
	}
	return "";
}

function bgpd_validate_input() {
	global $config, $g, $input_errors;


	if (!empty($_POST['asnum']) && !is_numeric($_POST['asnum']))
		$input_errors[] = "AS must be entered as a number only.";

	if (!empty($_POST['routerid']) && !is_ipaddr($_POST['routerid']))
		$input_errors[] = "Router ID must be an IP address.";

	if (!empty($_POST['holdtime']) && !is_numeric($_POST['holdtime']))
		$input_errors[] = "Holdtime must be entered as a number.";

	if (!empty($_POST['listenip']) && !is_ipaddr($_POST['listenip']))
		$input_errors[] = "Listen IP must be an IP address or blank to bind to all IPs.";

}

function bgpd_validate_group() {
	global $config, $g, $id, $input_errors;

	if (!is_numeric($_POST['remoteas']))
		$input_errors[] = "Remote AS must be entered as a number only.";

	if ($_POST['name'] == "")
		$input_errors[] = "You must enter a name.";

	$_POST['name'] = remove_bad_chars($_POST['name']);

}

function remove_bad_chars($string) {
	return preg_replace('/[^a-z|_|0-9]/i','',$string);
}

function grey_out_value_boxes() {
	echo <<<EOF
	<script language="javascript">
		function grey_out_value_boxes() {
			var x = 0;
			for(x=0; x<99; x++) {
				if( \$('parameters' + x) ) {
					var fieldvalue = $('parameters' + x).options[$('parameters' + x).selectedIndex].text;
					var length = fieldvalue.length;
					length = length -2;
					var last_two = fieldvalue.substring(length);
					var without_last_two = fieldvalue.substring(0,length);
					if( \$('parmvalue' + x) ) {
						if(last_two != ' X') {
							\$('parmvalue' + x).value = '';
							\$('parmvalue' + x).disabled = true;
						} else {
							\$('parmvalue' + x).disabled = false;
						}
					}
				}
			}
			var timerID  = setTimeout("grey_out_value_boxes()", 1200);

		}
		grey_out_value_boxes();
	</script>


EOF;

}

function is_openbgpd_running() {
	$status = `ps auxw | grep -c '[b]gpd.*parent'`;
	if(intval($status) > 0)
		return true;
	else
		return false;
}

?>
