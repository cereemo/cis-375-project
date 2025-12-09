path "transit/keys/jwt-key" {
  capabilities = ["read"]
}

path "transit/sign/jwt-key" {
  capabilities = ["update"]
}

path "transit/keys/jwt-key/config" {
  capabilities = ["read"]
}