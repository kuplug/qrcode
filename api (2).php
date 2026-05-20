<?php
/*
 * QR Scan Logger - PHP API
 * Endpoint: api.php?action={log|list|delete|stats}
 * Method: GET / POST / DELETE
 * Author: Wawan / PT Sismadi Langit Solusi
 */

// ============================================================
// KONFIGURASI DATABASE
// ============================================================
define('DB_HOST', 'localhost');
define('DB_NAME', 'qrscan_db');
define('DB_USER', 'root');
define('DB_PASS', '');

// ============================================================
// HEADER CORS & CONTENT TYPE
// ============================================================
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit();
}

// ============================================================
// KONEKSI DATABASE
// ============================================================
function getDB(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        try {
            $pdo = new PDO(
                "mysql:host=" . DB_HOST . ";dbname=" . DB_NAME . ";charset=utf8mb4",
                DB_USER,
                DB_PASS,
                [
                    PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
                    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                    PDO::ATTR_EMULATE_PREPARES   => false,
                ]
            );
        } catch (PDOException $e) {
            jsonError('Koneksi database gagal: ' . $e->getMessage(), 500);
        }
    }
    return $pdo;
}

// ============================================================
// ROUTER UTAMA
// ============================================================
$action = $_GET['action'] ?? '';
$method = $_SERVER['REQUEST_METHOD'];

// Baca body JSON untuk POST/DELETE
$body = [];
$rawBody = file_get_contents('php://input');
if (!empty($rawBody)) {
    $body = json_decode($rawBody, true) ?? [];
}

switch ($action) {

    case 'log':
        if ($method === 'POST') {
            createLog($body);
        } else {
            jsonError('Method harus POST untuk action log', 405);
        }
        break;

    case 'list':
        if ($method === 'GET') {
            getLogList();
        } else {
            jsonError('Method harus GET untuk action list', 405);
        }
        break;

    case 'delete':
        if ($method === 'DELETE' || $method === 'POST') {
            $id = (int)($_GET['id'] ?? $body['id'] ?? 0);
            deleteLog($id);
        } else {
            jsonError('Method harus DELETE untuk action delete', 405);
        }
        break;

    case 'stats':
        if ($method === 'GET') {
            getStats();
        } else {
            jsonError('Method harus GET untuk action stats', 405);
        }
        break;

    case 'install':
        // Inisialisasi tabel database (jalankan sekali)
        installDatabase();
        break;

    default:
        jsonError("Action '$action' tidak dikenal. Gunakan: log, list, delete, stats", 400);
}

// ============================================================
// CREATE: Simpan log scan baru
// ============================================================
function createLog(array $body): void {
    $qrData   = trim($body['qr_data']   ?? '');
    $deviceId = trim($body['device_id'] ?? 'unknown');
    $rssi     = (int)($body['rssi']     ?? 0);

    if (empty($qrData)) {
        jsonError('Field qr_data wajib diisi', 400);
        return;
    }

    // Batasi panjang data
    $qrData   = mb_substr($qrData, 0, 500);
    $deviceId = mb_substr($deviceId, 0, 100);

    $db = getDB();
    $stmt = $db->prepare("
        INSERT INTO scan_logs (qr_data, device_id, rssi, ip_address, created_at)
        VALUES (:qr_data, :device_id, :rssi, :ip, NOW())
    ");

    $stmt->execute([
        ':qr_data'   => $qrData,
        ':device_id' => $deviceId,
        ':rssi'      => $rssi,
        ':ip'        => $_SERVER['REMOTE_ADDR'] ?? '',
    ]);

    $id = (int)$db->lastInsertId();

    jsonSuccess([
        'id'         => $id,
        'qr_data'    => $qrData,
        'device_id'  => $deviceId,
        'message'    => 'Log scan berhasil disimpan',
    ]);
}

// ============================================================
// READ: Ambil daftar log scan
// ============================================================
function getLogList(): void {
    $db = getDB();

    // Query params opsional
    $limit     = min((int)($_GET['limit']     ?? 50), 200);
    $offset    = (int)($_GET['offset']    ?? 0);
    $deviceId  = $_GET['device_id'] ?? '';
    $startDate = $_GET['start_date'] ?? '';
    $endDate   = $_GET['end_date']   ?? '';
    $search    = $_GET['q']          ?? '';

    $where  = ['1=1'];
    $params = [];

    if (!empty($deviceId)) {
        $where[]               = 'device_id = :device_id';
        $params[':device_id']  = $deviceId;
    }
    if (!empty($startDate)) {
        $where[]               = 'DATE(created_at) >= :start_date';
        $params[':start_date'] = $startDate;
    }
    if (!empty($endDate)) {
        $where[]              = 'DATE(created_at) <= :end_date';
        $params[':end_date']  = $endDate;
    }
    if (!empty($search)) {
        $where[]          = 'qr_data LIKE :search';
        $params[':search'] = '%' . $search . '%';
    }

    $whereClause = implode(' AND ', $where);

    // Hitung total
    $countStmt = $db->prepare("SELECT COUNT(*) FROM scan_logs WHERE $whereClause");
    $countStmt->execute($params);
    $total = (int)$countStmt->fetchColumn();

    // Ambil data
    $params[':limit']  = $limit;
    $params[':offset'] = $offset;

    $stmt = $db->prepare("
        SELECT id, qr_data, device_id, rssi, ip_address, created_at
        FROM scan_logs
        WHERE $whereClause
        ORDER BY created_at DESC
        LIMIT :limit OFFSET :offset
    ");

    // Bind int params secara eksplisit untuk LIMIT/OFFSET
    foreach ($params as $key => $val) {
        if ($key === ':limit' || $key === ':offset') {
            $stmt->bindValue($key, $val, PDO::PARAM_INT);
        } else {
            $stmt->bindValue($key, $val);
        }
    }
    $stmt->execute();
    $data = $stmt->fetchAll();

    jsonSuccess([
        'data'   => $data,
        'total'  => $total,
        'limit'  => $limit,
        'offset' => $offset,
    ]);
}

// ============================================================
// DELETE: Hapus log berdasarkan ID
// ============================================================
function deleteLog(int $id): void {
    if ($id <= 0) {
        jsonError('ID tidak valid', 400);
        return;
    }

    $db   = getDB();
    $stmt = $db->prepare("DELETE FROM scan_logs WHERE id = :id");
    $stmt->execute([':id' => $id]);

    if ($stmt->rowCount() === 0) {
        jsonError("Log dengan ID $id tidak ditemukan", 404);
        return;
    }

    jsonSuccess(['id' => $id, 'message' => 'Log berhasil dihapus']);
}

// ============================================================
// READ: Statistik ringkas
// ============================================================
function getStats(): void {
    $db = getDB();

    $total = (int)$db->query("SELECT COUNT(*) FROM scan_logs")->fetchColumn();

    $hariIni = (int)$db->query("
        SELECT COUNT(*) FROM scan_logs WHERE DATE(created_at) = CURDATE()
    ")->fetchColumn();

    $deviceCount = (int)$db->query("
        SELECT COUNT(DISTINCT device_id) FROM scan_logs
    ")->fetchColumn();

    $lastScan = $db->query("
        SELECT created_at FROM scan_logs ORDER BY created_at DESC LIMIT 1
    ")->fetchColumn();

    // Scan per hari (7 hari terakhir)
    $perHari = $db->query("
        SELECT DATE(created_at) AS tanggal, COUNT(*) AS jumlah
        FROM scan_logs
        WHERE created_at >= DATE_SUB(NOW(), INTERVAL 7 DAY)
        GROUP BY DATE(created_at)
        ORDER BY tanggal ASC
    ")->fetchAll();

    jsonSuccess([
        'total_log'    => $total,
        'hari_ini'     => $hariIni,
        'device_count' => $deviceCount,
        'last_scan'    => $lastScan ?: '-',
        'per_hari'     => $perHari,
    ]);
}

// ============================================================
// INSTALL: Buat tabel jika belum ada
// ============================================================
function installDatabase(): void {
    $db = getDB();
    $db->exec("
        CREATE TABLE IF NOT EXISTS scan_logs (
            id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            qr_data    VARCHAR(500) NOT NULL,
            device_id  VARCHAR(100) NOT NULL DEFAULT 'unknown',
            rssi       SMALLINT    NOT NULL DEFAULT 0,
            ip_address VARCHAR(45) NOT NULL DEFAULT '',
            created_at DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_device  (device_id),
            INDEX idx_created (created_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
    ");

    jsonSuccess(['message' => 'Tabel scan_logs berhasil dibuat / sudah ada']);
}

// ============================================================
// HELPER: Output JSON sukses
// ============================================================
function jsonSuccess(array $data = [], int $code = 200): void {
    http_response_code($code);
    echo json_encode(array_merge(['status' => 'success'], $data), JSON_UNESCAPED_UNICODE);
    exit();
}

// ============================================================
// HELPER: Output JSON error
// ============================================================
function jsonError(string $message, int $code = 400): void {
    http_response_code($code);
    echo json_encode(['status' => 'error', 'message' => $message], JSON_UNESCAPED_UNICODE);
    exit();
}
