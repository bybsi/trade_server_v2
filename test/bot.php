<?php

$wait = 5; // seconds between each new batch of orders
require 'logging.php';
require 'db.php';
$table_name = 'tbl_trade_orders';
// TODO add a flag to not update currency hold table in the C program.

$redis = new Redis();
$redis->connect('127.0.0.1', 6379);
$tickers = ["ANDTHEN", "FORIS4", "SPARK", "ZILBIAN"];

while (1) {
	foreach ($tickers as $ticker) {
		$current_price = (float)$redis->get("$ticker-price");
		$variance = $current_price * 0.02;
		$amount = rand(1, 25);
		create_order($ticker, 'B', $amount, $current_price - $variance);
		create_order($ticker, 'S', $amount, $current_price + $variance);
	}

	sleep($wait);
}

function create_order($ticker, $side, $amount, $price) {
	global $_db, $table_name;
	$_db->insert($table_name, array(
		'user_id'=>5,
		'status'=>'O',
		'ticker'=>$ticker,
		'side'=>$side,
		'type'=>'L',
		'amount'=>$amount,
		'price'=>$price
	));
}

?>

