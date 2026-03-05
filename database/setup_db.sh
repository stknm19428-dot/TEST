#!/bin/bash

# 1. DB 이름 설정 (MariaDB는 파일이 아닌 데이터베이스 이름을 사용합니다)
DB_NAME="Smart_MES_Core"

echo "🚀 스마트팩토리 MES 데이터베이스 세팅을 시작합니다..."

# 2. MariaDB 명령 실행 (root 권한으로 실행)
sudo mariadb -u root <<EOF
-- 기존 DB 삭제 및 신규 생성
DROP DATABASE IF EXISTS $DB_NAME;
CREATE DATABASE $DB_NAME;
USE $DB_NAME;

-- [1. 테이블 생성 섹션]
-- MariaDB 호환을 위해 UUID 대신 VARCHAR(36)을 사용합니다.

CREATE TABLE process (
    id VARCHAR(36) PRIMARY KEY,
    process_name VARCHAR(50) NOT NULL
);

CREATE TABLE user (
    id VARCHAR(36) PRIMARY KEY,
    user_name VARCHAR(20) NOT NULL,
    role VARCHAR(20),
    process_id VARCHAR(36),
    rfid VARCHAR(20)
);

CREATE TABLE user_password (
    id VARCHAR(36) PRIMARY KEY,
    user_id VARCHAR(36) NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    salt VARCHAR(64) NOT NULL,
    FOREIGN KEY (user_id) REFERENCES user(id) ON DELETE CASCADE
);

CREATE TABLE supp_company (
    id VARCHAR(36) PRIMARY KEY,
    company_name VARCHAR(50) NOT NULL,
    company_address VARCHAR(50),
    company_number VARCHAR(20)
);

CREATE TABLE cust_company (
    id VARCHAR(36) PRIMARY KEY,
    company_name VARCHAR(50) NOT NULL,
    company_address VARCHAR(50),
    company_number VARCHAR(20)
);

CREATE TABLE inventory (
    id VARCHAR(36) PRIMARY KEY,
    company_id VARCHAR(36),
    item_code VARCHAR(50) UNIQUE,
    item_name VARCHAR(50) NOT NULL,
    current_stock INT,
    min_stock_level INT NOT NULL,
    max_stock_level INT,
    unit VARCHAR(20),
    location VARCHAR(10),
    FOREIGN KEY (company_id) REFERENCES supp_company(id)
);

CREATE TABLE product (
    id VARCHAR(36) PRIMARY KEY,
    product_code VARCHAR(50) UNIQUE,
    product_name VARCHAR(50) NOT NULL,
    product_stock INT,
    description TEXT
);

CREATE TABLE product_items (
    id VARCHAR(36) PRIMARY KEY,
    item_id VARCHAR(36),
    product_id VARCHAR(36),
    quantity_required INT NOT NULL,
    unit VARCHAR(20),
    location VARCHAR(10),
    FOREIGN KEY (item_id) REFERENCES inventory(id),
    FOREIGN KEY (product_id) REFERENCES product(id)
);

CREATE TABLE inventory_order_logs (
    id VARCHAR(36) PRIMARY KEY,
    user_id VARCHAR(36),
    item_id VARCHAR(36),
    stock INT,
    status VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (item_id) REFERENCES inventory(id)
);

CREATE TABLE product_order_logs (
    id VARCHAR(36) PRIMARY KEY,
    user_id VARCHAR(36),
    product_id VARCHAR(36),
    order_count INT,
    motor_speed INT,
    status VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(id),
    FOREIGN KEY (product_id) REFERENCES product(id)
);

CREATE TABLE product_deli_logs (
    id VARCHAR(36) PRIMARY KEY,
    company_id VARCHAR(36),
    product_id VARCHAR(36),
    delivery_stock INT,
    status VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (company_id) REFERENCES cust_company(id),
    FOREIGN KEY (product_id) REFERENCES product(id)
);

CREATE TABLE product_logs (
    id VARCHAR(36) PRIMARY KEY,
    order_id VARCHAR(36),
    user_id VARCHAR(36),
    assignment_part VARCHAR(50),
    motor_speed INT,
    prod_count INT,
    defect_count INT,
    status VARCHAR(20),
    started_at TIMESTAMP NULL DEFAULT NULL,
    ended_at TIMESTAMP NULL DEFAULT NULL,
    FOREIGN KEY (order_id) REFERENCES product_order_logs(id),
    FOREIGN KEY (user_id) REFERENCES user(id)
);

CREATE TABLE environment_logs (
    id VARCHAR(36) PRIMARY KEY,
    process_id VARCHAR(36),
    sensor_type VARCHAR(20),
    sensor_value VARCHAR(100),
    event_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (process_id) REFERENCES process(id)
);

-- [2. 데이터 삽입 섹션]

INSERT INTO process (id, process_name) VALUES ('p1', 'Main_Assembly_Line');

INSERT INTO supp_company (id, company_name, company_address, company_number) VALUES 
('s1', '알루미늄테크', '대구', '032-1111-2222'),
('s2', '유라코퍼레이션','성남', '031-1234-3222'),
('s3', '보쉬전장', '세종', '044-013-0630'),
('s4', '두원공조', '아산', '041-014-0730');

INSERT INTO cust_company (id, company_name, company_address, company_number) VALUES 
('c1', '현대자동차', '울산', '02-123-4567'),
('c2', '기아자동차', '광주', '02-124-5678');

INSERT INTO user (id, user_name, role, process_id, rfid) VALUES 
('u1', 'product_park', 'MANAGER', 'p1', 'RF12345678'),
('u2', 'inventory_ahn', 'Assist.MANAGER', 'p1', 'RF02050205');

INSERT INTO user_password (id, user_id, password_hash, salt) VALUES 
('up1', 'u1', 'hashed_password_123', 'salt-01'),
('up2', 'u2', 'hashed_password_000', 'salt-02');

INSERT INTO inventory (id, company_id, item_code, item_name, current_stock, min_stock_level, max_stock_level, unit, location) VALUES
('inv1', 's1', 'i1', 'Aluminum_Frame', 450, 100, 1000, 'A', 'HUB_A'),
('inv2', 's2', 'i2', 'Harness', 200, 100, 1000, 'A', 'HUB_A'),
('inv3', 's3', 'i3', 'ECF', 100, 100, 700, 'A', 'HUB_B'),
('inv4', 's4', 'i4', 'Radiator', 100, 100 ,700, 'A', 'HUB_B');

INSERT INTO inventory_order_logs (id, user_id, item_id, stock, status, created_at) VALUES 
('io-log1', 'u1', 'inv1', 100, 'PENDING', '2026-02-28'),
('io-log2', 'u2', 'inv2', 300, 'PENDING', '2026-02-28');

INSERT INTO product (id, product_code, product_name, product_stock, description) VALUES 
('prod1', 'CS600-1', 'JK1-600W_CoolingModule', 50, 'CM_for_GV70'),
('prod2', 'ICCB-1', 'QV1-ICCB', 100, 'ICCB_for_EV2');

INSERT INTO product_items (id, item_id, product_id, quantity_required, unit, location) VALUES 
('bom1', 'inv1', 'prod1', 300, 'EA', 'L1'),
('bom2', 'inv2', 'prod2', 500, 'EA', 'L2');

INSERT INTO product_order_logs (id, user_id, product_id, order_count, motor_speed, status, created_at) VALUES
('po1', 'u1', 'prod1', 300, 50, 'IN_PROGRESS','2026-02-25'),
('po2', 'u2', 'prod1', 500, 70, 'IN_PROGRESS','2026-03-01');

INSERT INTO product_deli_logs (id, company_id, product_id, delivery_stock, status, created_at) VALUES
('pd1', 'c1', 'prod1', 50, 'COMPLETED', '2026-02-15'),
('pd2', 'c2', 'prod2', 100, 'COMPLETED', '2026-02-18');

INSERT INTO product_logs (id, order_id, user_id, assignment_part, motor_speed, prod_count, defect_count, status, started_at) VALUES
('prod_live1', 'po1', 'u1', 'Assembly', 50, 100, 5, 'IN_PROGRESS', '2026-02-26'),
('prod_live2', 'po2', 'u2', 'Assembly', 70, 300, 10, 'IN_PROGRESS', '2026-03-02');

INSERT INTO environment_logs (id, process_id, sensor_type, sensor_value) VALUES
('env1', 'p1', 'TEMP', '26.5'), ('env2', 'p1', 'FLAME', '0');
EOF

echo "✅ 모든 테이블 생성 및 데이터 삽입이 완료되었습니다."
echo "🔎 재고 현황 확인:"
sudo mariadb -u root -e "USE $DB_NAME; SELECT item_name, current_stock, location FROM inventory;"