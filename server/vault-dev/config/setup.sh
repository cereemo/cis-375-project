#!/bin/sh

apk add --no-cache jq

export VAULT_ADDR=http://vault:8200

echo "Waiting for Vault to start..."
until vault status > /dev/null 2>&1 || [ $? -eq 2 ]; do
  sleep 1
done

if [ ! -f /vault/data/keys.json ]; then
  echo "Initializing Vault..."
  vault operator init -key-shares=1 -key-threshold=1 -format=json > /vault/data/keys.json
fi

UNSEAL_KEY=$(jq -r ".unseal_keys_b64[0]" /vault/data/keys.json)
vault operator unseal "$UNSEAL_KEY"

ROOT_TOKEN=$(jq -r ".root_token" /vault/data/keys.json)
vault login "$ROOT_TOKEN"

echo "Vault is unsealed and logged in."

vault secrets enable transit || true
vault write -f transit/keys/jwt-key type=ed25519 || true
vault write transit/keys/jwt-key/config auto_rotate_period=24h
vault auth enable approle || true
vault policy write jwt-app-policy /vault/config/app-policy.hcl
vault write auth/approle/role/drogon-server-role \
    token_policies="default,jwt-app-policy" \
    token_ttl=1h \
    token_max_ttl=4h

echo "Configuration complete!"
echo "Fetching credentials..."

ROLE_ID=$(vault read -field=role_id auth/approle/role/drogon-server-role/role-id)
SECRET_ID=$(vault write -f -field=secret_id auth/approle/role/drogon-server-role/secret-id)

echo ""
echo "ROLE_ID: $ROLE_ID"
echo "SECRET_ID: $SECRET_ID"