#!/bin/bash

# 1. DB 파일 이름 설정
DB_NAME="smart_factory.db"

echo "🚀 스마트팩토리 MES 데이터베이스 세팅을 시작합니다..."

# 2. 기존 데이터베이스 파일 삭제 (초기화 테스트용)
if [ -f "$DB_NAME" ]; then
    rm "$DB_NAME"
    echo "♻️ 기존 DB 파일을 삭제했습니다."
fi

mariadb $DB_NAME <<EOF
[데이터 삽입]

-- [마스터 데이터] 공정 및 업체
INSERT INTO process (id, process_name) VALUES ('p1', 'Main_Assembly_Line');

INSERT INTO supp_company (id, company_name, company_address, company_number) VALUES 
('s1', '알루미늄테크', '대구', '032-1111-2222'),
('s2', '유라코퍼레이션','성남' '031-1234-3222'),
('s3', '보쉬전장', '세종', '044-013-0630'),
('s4', '두원공조', '아산', '041-014-0730');

INSERT INTO CUST_company (id, company_name, company_address, company_number) VALUES 
('c1', '현대자동차', '울산' '02-123-4567'),
('c2', '기아자동차', '광주' '02-124-5678');


-- [유저 데이터] RFID 포함
INSERT INTO user (id, user_name, role, process_id, rfid) VALUES 
('u1', 'product_park', 'MANAGER', 'pro1', 'RF12345678'),
('u2', 'inventory_ahn', 'Assist.MANAGER', 'inv1', 'RF02050205');
INSERT INTO user_password (id, user_id, password_hash, salt) VALUES 
('up1', 'u1', 'hashed_password_123', 'salt-01'),
('up1', 'u2', 'hashed_password_000', 'salt-02'),


-- [물류]
INSERT INTO inventory (id, company_id, item_code, item_name, current_stock, min_stock_level, max_stock_level, unit, location) VALUES
('inv1', 's1', 'i1', 'Aluminum_Frame', 450, 100, 1000, 'A', 'HUB_A'),
('inv2', 's2', 'i2', 'Harness', 200, 100, 1000, 'A', 'HUB_A'),
('inv3', 's3', 'i3', 'ECF', 100, 100, 700, 'A', 'HUB_B'),
('inv4', 's4', 'i4', 'Radiator', 100, 100 ,700, 'A', 'HUB_B');

INSERT INTO inventory_order_logs (id, user_id, item_id, stock, status, created_at, updated_at) VALUES 
('io-log1', 'u1', 'inv1', 100, 'PENDING', '02-28', '03-01'),
('io-log2', 'u2', 'inv2', 300, 'PENDING', '02-28', '03-01');

-- [제조 데이터] 제품 및 BOM
INSERT INTO product (id, product_code, product_name, product_stock, description) VALUES 
('prod1', 'CS600-1', 'JK1-600W_CoolingModule', 50, 'CM_for_GV70'),
('prod2', 'ICCB-1' 'QV1-ICCB', 100, 'ICCB_for_EV2');

INSERT INTO product_items (id, item_id, product_id, quantity_required, unit, location) VALUES 
('bom1', 'inv1', 'prod1', 300. 'EA', 'L1'),
('bom2', 'inv2', 'prod2', 500, 'EA', 'L2');

INSERT INTO product_order_logs (id, user_id, product_id, order_count, motor_speed, status, created_at, updated_at) VALUES
('po1', 'u1', 'prod1', 300, 50, 'IN_PROGRESS','02-25','02-26'),
('po2', 'u2', 'prod1', 500, 70, 'IN_PROGRESS','03-01','03-03');

INSERT INTO product_deli_logs (id, company_id, product_id, delivery_stock, status, created_at, updated_at) VALUES
(pd1, c1, prod1, 50, 'COMPLETED', '02-15', '02-24'),
(pd2, c2, prod2, 100, 'COMPLETED', '02-18', '03-01');


-- [실시간 로그] 환경 및 생산
INSERT INTO product_logs (id, order_id, user_id, assignment_part, motor_speed, prod_count, defect_count, status, started_at, ended_at) VALUES
(prod_live1, pd1, u1, '?', 50, 100, 5, 'IN_PROGRESS', '02-26', '03-07'),
(prod_live2, pd2, u2, '?', 70, 300, 10, 'IN_PROGRESS', '03-02', '03-10');

INSERT INTO process (id, process_name) VALUES (env_pro1, TEMP), (env_pro2, FLAME);

INSERT INTO environment_logs (id, process_id, sensor_type, sensor_value) VALUES
('env1', 'p1', 'TEMP', '26.5'), ('env2', 'p1', 'FLAME', '0');
EOF

echo "✅ 모든 데이터가 성공적으로 삽입되었습니다."
echo "🔎 확인을 위해 현재 재고 현황을 출력합니다:"
sqlite3 $DB_NAME "SELECT item_name, current_stock, location FROM inventory;"
