<?php
$logging = true;
$log_file = 'bot_log.log';
$lock_file = 'bot_log.lock';
function bs_log($message) {
	global $logging;
	global $log_file;
	global $lock_file;
	if (!$logging)
		return;
	try {
		$lock_fh = fopen($lock_file, "w");
	} catch (Exception $exc) {
		echo "$exc\n";
		exit(1);
	}
	flock($lock_fh, LOCK_EX);
		$log_fh = fopen($log_file, "a");
		flock($log_fh, LOCK_EX);
		fwrite($log_fh, sprintf("[%s] %s\n", date('Y-m-d H:i:s'), $message));
		fflush($log_fh);
		flock($log_fh, LOCK_UN);
		fclose($log_fh);
	flock($lock_fh, LOCK_UN);
	fclose($lock_fh);
}
?>
