SELECT AVG(i_price) FROM item_sample;
UPDATE item_sample SET i_price = i_price * 1.1 WHERE i_id > 500;
SELECT AVG(i_price) FROM item_sample;
