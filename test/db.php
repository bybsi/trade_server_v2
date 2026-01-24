<?php
abstract class DB2 {

	protected $host;
	protected $db_name;
	protected $user;
	protected $pass;
	protected $db;
	
	public function __construct($host, $db_name, $user, $pass)
	{
		$this->host = $host;
		$this->db_name = $db_name;
		$this->user = $user;
		$this->pass = $pass;
		$this->db = NULL;
	}
	
	protected function sanitize_arr(&$arr, $regex = '/[^\w]/') {
		for ($i = 0; $i < count($arr); $i++)
			$arr[$i] = preg_replace($regex, '', $arr[$i]);
	}
	
	protected function close() {
		//$this->db->close();
		$this->pdo = null;
	}

	public abstract function connect();
	public abstract function query_first($sql, $query_name, $params);
	public abstract function query_all($sql, $query_name, $params);
	public abstract function delete($table_name, $sql, $conditions);
	public abstract function insert($table_name, $values);
	public abstract function insert_id($table_name, $cols, $values);
	public abstract function upsert($table_name, $cols, $conflict_cols, $values);
	public abstract function update($table_name, $sql, $values);
	public abstract function execute($query_name, $sql, $params);
	public abstract function get_error();
}

class PDODB extends DB2 {

	private $db_type = "mysql";

	function __construct($host, $db_name, $user, $pass) {
		parent::__construct($host, $db_name, $user, $pass);
	}
	
	public function connect()
	{
		try {
			$options = array(
				PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
				PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
				PDO::ATTR_EMULATE_PREPARES => false
			);
			$this->db = new PDO(
				"{$this->db_type}:host={$this->host};dbname={$this->db_name}", 
				$this->user, $this->pass, $options);
		} catch (Exception $e) {
			bs_log($e->getMessage());
			die("Could not connect to db");
		}
	}
	
	public function query_first($sql, $query_name, $params = array()) {
		try {
			$query = $this->db->prepare($sql);
			$query->execute($params);
			return $query->fetch();
		} catch (Exception $e) {
			bs_log("DB ERROR:$query_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
	}
	
	public function query_all($sql, $query_name, $params = array()) {
		try {
			$query = $this->db->prepare($sql);
			$query->execute($params);
			return $query->fetchAll(PDO::FETCH_ASSOC);
		} catch (Exception $e) {
			bs_log("DB ERROR:$query_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
	}
	
	public function delete($table_name, $sql, $params = array()) {
		try {
			$query = $this->db->prepare($sql);
			$query->execute($params);
			$count = $query->rowCount();
			if ($count > 0) {
				bs_log("Deleted $count $table_name records.");
			}
		} catch (Exception $e) {
			bs_log("DB ERROR:delete from $table_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return true;
	}

	public function insert($table_name, $params) {
		try {
			$values = array();
			$columns = array();
			foreach ($params as $k => $v) {
				$columns[] = $k;
				$values[] = $v;
			}
			$qmarks = array_fill(0, count($values), '?');
			$sql = "INSERT INTO $table_name ";
			$sql .= "(" . implode(',', $columns) . ") ";
			$sql .= "VALUES(" . implode(',', $qmarks) . ")";
			$query = $this->db->prepare($sql);
			$query->execute($values);
		} catch (Exception $e) {
			bs_log("DB ERROR:insert $table_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return true;
	}
	
	public function upsert($table_name, $cols, $conflict_cols, $values) {
		$this->sanitize_arr($cols);
		$this->sanitize_arr($values, '/[^\w.]/');
		$this->sanitize_arr($conflict_cols);
		try {
			$update_str = "";
			foreach ($cols as $col) {
				$update_str .= "$col=VALUES($col),";
			}
			$update_str = rtrim($update_str, ',');
			
			$sql = "
INSERT INTO $table_name 
(".implode(',', $cols).") 
VALUES('".implode("','", $values)."')
ON DUPLICATE KEY UPDATE $update_str
";
			$query = $this->db->prepare($sql);
			$query->execute();
			$count = $query->rowCount();
			if ($count > 0) {
				bs_log("Upsert handled $count $table_name records.");
			}
		} catch (Exception $e) {
			bs_log("Upsert error: " . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return true;
	}

	public function upsert_increment($table_name, $cols, $vals, $update_cols) {
		$this->sanitize_arr($cols);
		$this->sanitize_arr($vals, '/[^\w]/');
		$this->sanitize_arr($update_cols, '/[^\w]/');
		try {
			$update_str = "";
			for ($i = 0; $i < count($cols); $i++) {
				if (in_array($cols[$i], $update_cols))
					$update_str .= $cols[$i] . '=' . $cols[$i] . '+' . $vals[$i] . ',';
			}
			$update_str = rtrim($update_str, ',');
			
			$sql = "
INSERT INTO $table_name 
(".implode(',', $cols).") 
VALUES('".implode("','", $vals)."')
ON DUPLICATE KEY UPDATE $update_str
";
			$query = $this->db->prepare($sql);
			$query->execute();
			$count = $query->rowCount();
			if ($count > 0) {
				bs_log("Upsert handled $count $table_name records.");
			}
		} catch (Exception $e) {
			bs_log("Upsert increment error: " . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return true;
	}
	
	public function insert_id($table_name, $cols, $values) {
		$this->sanitize_arr($cols);
		$this->sanitize_arr($values, '/[^\w. :-]/');
		
		try {
			$sql = "INSERT INTO $table_name (".implode(',', $cols).") VALUES('".implode("','", $values)."')";
			$query = $this->db->prepare($sql);
			$query->execute();
			return $this->db->lastInsertId();
		} catch (Exception $e) {
			bs_log("DB ERROR:insert_id $table_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return $this->db->lastInsertId();
	}

	public function update($table_name, $sql, $values) {
		try {
			$query = $this->db->prepare($sql);
			$query->execute($values);
		} catch (Exception $e) {
			bs_log("DB ERROR:update $table_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return true;
	}

	public function execute($query_name, $sql, $params) {
		try {
			$query = $this->db->prepare($sql);
			$query->execute($params);
		} catch (Exception $e) {
			bs_log("DB ERROR:execute $table_name:" . $e->getMessage());
			bs_log($this->get_error());
			return false;
		}
		return true;
	}
	
	public function get_error() {
		return $pdo->errorInfo();
	}
}

$format = "\nHOST\nDBNAME\nUSERNAME\nPASSWORD\n";
$lines = file('.db', FILE_IGNORE_NEW_LINES);
if ($lines === false) {
	echo ".db file not found, format: $format";
	exit(1);
}
if (count($lines) != 4) {
	echo "Invalid format: $format";
	exit(1);
}

$_db = new PDODB($lines[0], $lines[1], $lines[2], $lines[3]);
$_db->connect();

?>
