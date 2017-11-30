SELECT W_ID, COUNT(DISTINCT I_IM_ID), COUNT(*)
FROM warehouse w
JOIN stock_sample s ON w.W_ID = s.S_W_ID
JOIN item_sample i ON i.I_ID = s.S_I_ID
GROUP BY w_id;
